/* 
 * Copyright (C) 2000-2004 KЕre SjЖlander <kare@speech.kth.se>
 * Copyright (C) 1997 Philippe Langlais <felipe@speech.kth.se>
 *
 * This file is part of the Snack Sound Toolkit.
 * The latest version can be found at http://www.speech.kth.se/snack/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "tcl.h"
#include "snack.h"

extern int Get_f0(Sound *s, Tcl_Interp *interp, int objc,
                  Tcl_Obj *CONST objv[]);

/* ********************************************************************* */
/*                LES PARAMETRES GLOBAUX DU DETECTEUR                    */
/* ********************************************************************* */

#define INFO 1

/* #define CORRECTION pour faire une adaptation de la fo */


#define SEUIL_NRJ 40 /* on coupe si on a pas XX% de la dyn. de l'energie */
#define SEUIL_DPZ 50 /* on coupe si on a plus que YY% de la dyn de dpz */ 
#define EPSILON   10 /* seuil pour le calcul de la dpz */

#define SEUIL_VOISE       7
#define AVANCE            2

#define cst_pics_amdf        5    /* max. number of amdf values per trame */
#define PITCH_VARIABILITY    25   /* percentage of pitch-variability between 2 values */
#define FILTRE_PASSE_BAS     5    /* number of time it should apply the high frequency filter */


#define POURCENT(n,t) ( (t)? (((n)*100) /(t)) : 0 )
#define CARRE(a) ((a)*(a))
#define TO_FREQ(p) ((p)? (cst_freq_ech / (p)) : 0)
#define minimum(a,b) (((a)>(b))? (b) : (a))
#define maximum(a,b) (((a)>(b))? (a) : (b))

#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

#define PI_2 6.28318530717958
#define MAX_ENTIER            2147483


#define NON_VOISEE(t)     (Vois[t] < SEUIL_VOISE)
#define VOISEE(t)         (Vois[t] >= SEUIL_VOISE)


/* ********************************************************************* */
/*                    Quelques declarations anodines                     */
/* ********************************************************************* */

typedef struct
  {
  int total;
  int rang;
  } RESULT;

typedef struct bidon
  {
  int debut,fin,ancrage;
  struct bidon *suiv,*pred;
  } *ZONE;


/* ********************************************************************* */
/*                LES VARIABLES GLOBALES DU DETECTEUR                    */
/* ********************************************************************* */


static int min_fo,max_fo,min_nrj,max_nrj,min_dpz,max_dpz;
static int nb_coupe=0,debug,quick;
static int seuil_dpz,seuil_nrj;

/*
  
  hamming window length = 2.5 * frequency / min_fo (16kHz,60Hz = 667)
  window skip           = frequency / 100          (16kHz      = 160)

  */

static int      cst_freq_ech,
         cst_freq_coupure,
  	 cst_length_hamming, 
         cst_step_hamming,
	 cst_point_par_trame,
 	 cst_step_min,
	 cst_step_max;


static RESULT	      *(Coeff_Amdf[cst_pics_amdf]);
static ZONE          zone;
static double	      *Hamming;
static int           max_amdf,min_amdf,amplitude_amdf;
static short         *Nrj,*Dpz,*Vois,*Fo;
static float         *Signal;
static int           **Resultat;


/* *********************************************************************** */
static void init(int frequence,int cst_min_fo,int cst_max_fo)
{  
  cst_freq_coupure     = 800;
  cst_freq_ech         = frequence;
  cst_length_hamming   = ( (int) (2.5 * cst_freq_ech) / cst_min_fo );
  cst_step_hamming = cst_point_par_trame  = ((int) (cst_freq_ech / 100));
  cst_step_min     = ( cst_freq_ech / cst_max_fo);
  cst_step_max     = ( cst_freq_ech / cst_min_fo);
  
  if (debug > 1) 
    printf("sampling:%d, hamming size=%d, hamming overlap=%d\n",
           cst_freq_ech,cst_length_hamming,cst_step_hamming);
  }

 /* *********************************************************************** */
/*
static void filtre_passe_bas(int frequence,int longueur)
  {
  int i;
  double coeff,
	 delai=0.0;
  coeff = ( PI_2 * frequence ) / cst_freq_ech;
  for (i=0;i<longueur;i++)
    Signal[i] = (short) (delai = (double) (Signal[i] * coeff) + (delai * (1-coeff)));
  }
  */
/* *********************************************************************** */

static void libere_zone(ZONE zone)
  {
  ZONE l;

  while (zone)
    {
    l=zone->suiv;
    ckfree((char *) zone);
    zone = l;
    }

  }

/* *********************************************************************** */
static void libere_coeff_amdf(void)
  {
  int i;
  for (i=0;i<cst_pics_amdf;i++) ckfree((char *) Coeff_Amdf[i]);
  }

/* *********************************************************************** */

static int voisement_par_profondeur_des_pics(int imin,int *result,int length)
  {
  int gauche,droite,i;
  
  for (i=imin; i>0 && result[i] <= result[i-1]; i--);          /* emergence gauche */
  gauche = result[i] - result[imin];
  for (i=imin; i<length-1 && result[i] <= result[i+1]; i++);   /* emergence droite */
  droite = result[i] - result[imin];

  return minimum(droite,gauche);
 }

/* *********************************************************************** */
static void ranger_minimum(int trame, int valeur,int rang)
  {
  int i,j;

  for (i=0; (i<cst_pics_amdf) && (valeur>=Coeff_Amdf[i][trame].total);i++);
  if (i<cst_pics_amdf)
    {
    for (j=cst_pics_amdf-1;j>i;j--)
      Coeff_Amdf[j][trame]=Coeff_Amdf[j-1][trame];
    Coeff_Amdf[i][trame].total = valeur;
    Coeff_Amdf[i][trame].rang  = rang;
    }
  }


/* *********************************************************************** */

static void retiens_n_pics(int *result,int trame,int length,int *profondeur)
  {
#define homothetie(y,minval,amp)     (int) ( (amp)? (((y)-(minval)) * 200 / (amp)) : 0 )

  int i,prof,profondeur_locale=0;
  int *result_local;
  int minVal,maxVal;
  
  for (i=0;i<cst_pics_amdf;i++)                  /* ----- init ------ */
    {
    Coeff_Amdf[i][trame].total = MAX_ENTIER ;
    Coeff_Amdf[i][trame].rang  = -1;
    }

  for (i=0; i<length-1; )                    /* ----- minimum ----- */
    {
    while ( (i<length-1) && (result[i]>result[i+1]) ) i++;   /* on descend */
    if (i && i<length-1) ranger_minimum(trame,result[i],i+cst_step_min);
    while ( (i<length-1) && (result[i]<=result[i+1]) ) i++;  /* on monte */
    }

  /* -------- Recherche du voisement par profondeur du pic minimum --------- */

  prof = *profondeur = 0;

  {
  result_local = (int *) ckalloc(sizeof(int)*length);
  maxVal=0; 
  minVal = MAX_ENTIER;
  for (i=0;i<length;i++)
    {
    if (result[i] > maxVal) maxVal = result[i];
    if (result[i] < minVal) minVal = result[i];
    }
  if (debug > 1) printf("DYN AMDF[%d] : %d - %d (%d)  ",trame,minVal,maxVal,maxVal-minVal);
  }

  for (i=0;i<length;i++) 
    {
    result_local[i] = homothetie(result[i],minVal,maxVal-minVal);
    result[i] = homothetie(result[i],min_amdf,amplitude_amdf);
    }

  for (i=0; i<cst_pics_amdf; i++)
    if (Coeff_Amdf[i][trame].rang != -1) 
         {
	 prof = voisement_par_profondeur_des_pics( Coeff_Amdf[i][trame].rang-cst_step_min,
						   result,
						   length);
         if ( prof > *profondeur) *profondeur = prof;
	 prof = voisement_par_profondeur_des_pics( Coeff_Amdf[i][trame].rang-cst_step_min,
						   result_local,
						   length);
         if ( prof > profondeur_locale) profondeur_locale = prof;
         }

  Vois[trame] = profondeur_locale;
  ckfree((char *) result_local);
  if (debug>1) printf("----> %d\n",*profondeur);
#undef homothetie
  }

/* *********************************************************************** */

static void calcul_voisement(int nb_trames)
{
  int trame, profondeur, length;
  
  amplitude_amdf = max_amdf - min_amdf;

  length = cst_step_max-cst_step_min+1;
 
  for (trame = 0; trame < nb_trames; trame++)
    {
    if (quick && (Nrj[trame] < seuil_nrj) && (Dpz[trame] > seuil_dpz)) Vois[trame] = 0;
    else
      {
      retiens_n_pics(Resultat[trame],trame,length,&profondeur);
      Vois[trame] = profondeur;
      }
    }

  }

/* *********************************************************************** */

static void precalcul_hamming(void)
  {
  double pas;
  int i;

  pas = PI_2 / cst_length_hamming;
  for (i=0;i<cst_length_hamming;i++)
    {
    Hamming[i] = 0.54 - 0.46 * cos(i*pas);
    }
  }

/*еееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееее*/

static void
amdf(Sound *s, int i, int *Hammer, int *result, int nrj, int start)
{
#define FACTEUR 50

  int decal,j,l,somme,k;
  double coeff, delai;
  static double odelai[FILTRE_PASSE_BAS];

  Snack_GetSoundData(s, start+i, Signal, cst_length_hamming);

  if (i == 0) {
    for (k = 0; k < FILTRE_PASSE_BAS; k++) {
      odelai[k] = 0.0;
    }
  }
  
  for (k = 0; k < FILTRE_PASSE_BAS; k++) {
    delai = odelai[k];
    coeff = ( PI_2 * cst_freq_coupure ) / cst_freq_ech;
    for (j = 0; j < cst_length_hamming; j++) {
      Signal[j] = (float) (delai = (double) (Signal[j] * coeff)
			   + (delai * (1-coeff)));
    }
    odelai[k] = (double) Signal[cst_step_hamming-1];
  }

  for (j=0; j<cst_length_hamming; j++) {
    Hammer[j] = (int) (Hamming[j]*Signal[j]);
  }
  
  /* ---- la double boucle couteuse --- */
  for (decal=cst_step_min; decal <= cst_step_max; decal++) {
    for (somme=l=0,j=decal; j<cst_length_hamming; j++,l++) {
      somme += abs( Hammer[j] - Hammer[l]);
    }
    result[decal-cst_step_min] = (int) ( (somme * FACTEUR) / ((cst_length_hamming-decal)));
  }
  
#undef FACTEUR
  }
/* *********************************************************************** */
static int
parametre_amdf(Sound *s, Tcl_Interp *interp, int start, int longueur,
	       int *nb_trames,int *Hammer)
{
  int j,i,length,trame;

  max_amdf = 0;
  min_amdf = MAX_ENTIER;

  length = cst_step_max-cst_step_min+1;

  for (trame=i=0; i < longueur; i += cst_step_hamming,trame++) {
    if (i > s->length - cst_length_hamming) break;
    if (i > longueur - cst_length_hamming/2) break;
    if (quick && (Nrj[trame] < seuil_nrj) && (Dpz[trame] > seuil_dpz)) {
    } else {
      amdf(s, (int) i,Hammer,Resultat[trame],(Nrj[trame])? Nrj[trame]:1,start);
      for (j=0; j<length;j++) {
	if (Resultat[trame][j]>max_amdf) max_amdf = Resultat[trame][j];
	
	if (Resultat[trame][j]<min_amdf) min_amdf = Resultat[trame][j];
      }
    }
    if ((trame % 20) == 19) {
      int res = Snack_ProgressCallback(s->cmdPtr, interp, "Computing pitch",
				       0.05 + 0.95 * (double) i / longueur);
      if (res != TCL_OK) {
	return TCL_ERROR;
      }
    }
  }
  Snack_ProgressCallback(s->cmdPtr, interp, "Computing pitch", 1.0);
  *nb_trames = (int) trame;

  if (debug) printf("min_amdf=%d, max_amdf=%d\n",min_amdf,max_amdf);
  return TCL_OK;
}

/* ************************************************************************ */
/*             LA MAGOUILLE DE LA RECHERCHE DU MEILLEUR CHEMIN              */
/* ************************************************************************ */

static int interpolation(int x1,int y1,int x2,int y2,int x)
  {
  int a,b,delta;
  
  /* y = ax+b */
  
  if (x==x1) return y1;
  if (x==x2) return y2;

  if ((delta = x1-x2))
    {
    a = y1 -y2 ;
    b = x1 * y2 - x2 * y1;
    
    return (a*x +b) / delta;
    }
  return 0;  
  }  
/* *********************************************************************** */

static int ecart_correct(int avant,int apres)
  {
  int a,b;

  a  = cst_freq_ech / maximum(avant,apres);
  b  = cst_freq_ech / minimum(avant,apres);


  return  ( b    <=   ((a * (100+PITCH_VARIABILITY)) / 100)  ) ;

  }

/* *********************************************************************** 
   selection du pic le plus proche d'une reference 
   еееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееееее */

void trier(int trame,int reference,RESULT *table)
  {
#define TABLEAU(t) (abs(table[t].rang-reference))
  int t,bulle=1;


  for (t=0;t<cst_pics_amdf;t++)
    table[t] = Coeff_Amdf[t][trame];

  while (bulle)
    for (bulle=t=0;t<cst_pics_amdf-1;t++)
      if ( (table[t].rang == -1 && table[t+1].rang != -1) || 
           (TABLEAU(t) > TABLEAU(t+1) && table[t+1].rang != -1) )
	{
	RESULT temp;
	temp       = table[t+1];
	table[t+1] = table[t];
	table[t]   = temp;
	bulle      = 1;
	}

#undef TABLEAU
  }

/* ***********************************************************************  */

static void extension_zone(int accrochage,int debut,int fin,int to,short *tableau)
  {
#define POURCENTAGE 25
#define SEUIL 20  
#define MAUVAIS_CHEMIN()
#define ECART_CORRECT(a,b) ( (maximum(a,b) - minimum(a,b)) < minimum(SEUIL,(POURCENTAGE * minimum(a,b) / 100)) )

  int trame,avant,i,m,j;
  RESULT table[AVANCE+1][cst_pics_amdf];
  RESULT normal[cst_pics_amdf];

  tableau[accrochage] = avant = to;

/* ---------------on etend a gauche ---------------------------*/

  for (trame=accrochage;trame>debut;)
    {
    table[0][0].rang = avant;
    for (i=1;i<=AVANCE && trame-i >=debut; i++)  
      trier(trame-i,table[i-1][0].rang,table[i]);
    trier( m=maximum(trame-AVANCE , debut) ,avant,normal);
  
    i--;
    
    if ( table[i][0].rang != -1 && normal[0].rang != -1  && table[i][0].rang && normal[0].rang  && avant &&
           abs(TO_FREQ(table[i][0].rang) - TO_FREQ(avant)) > abs(TO_FREQ(normal[0].rang) - TO_FREQ(avant))
       )
        {
        for (j=1;j<=i; j++)  
          tableau[trame-j] = interpolation(trame,avant,m,normal[0].rang,trame-j);
        trame -= i;
        avant = normal[0].rang;
        }
    else  
        {
        if ( table[1][0].rang && ECART_CORRECT(TO_FREQ(avant),TO_FREQ(table[1][0].rang)))
          avant = tableau[--trame] = table[1][0].rang;
        else tableau[--trame] = 0;
        }
    }
    

/* ------------------ on etend a droite -----------------------*/

  avant = tableau[accrochage];
  for (trame=accrochage;trame<fin;)
    {
    table[0][0].rang = avant;
    for (i=1;i<=AVANCE && trame+i <= fin; i++)  
      trier(trame+i,table[i-1][0].rang,table[i]);
    trier( m = minimum(trame+AVANCE , fin) ,avant,normal);
          
    i--;

    if ( table[i][0].rang != -1 && normal[0].rang != -1 && table[i][0].rang && normal[0].rang  && avant &&
         abs(TO_FREQ(table[i][0].rang) - TO_FREQ(avant)) > abs(TO_FREQ(normal[0].rang) - TO_FREQ(avant))
       )
        { 
        for (j=1;j<=i; j++)
          tableau[trame+j] = interpolation(trame,avant,m,normal[0].rang,trame+j);
        trame += i;
        
        avant = normal[0].rang;
        }
    else  
        {
        if ( table[1][0].rang && ECART_CORRECT(TO_FREQ(avant),TO_FREQ(table[1][0].rang)))
          avant = tableau[++trame] = table[1][0].rang;
        else tableau[++trame] = 0;
        }
    }

#undef MAUVAIS_CHEMIN
#undef SEUIL 
#undef ECART_CORRECT
#undef POURCENTAGE
  }
/* ***********************************************************************  */

static void recupere_trou(ZONE l,short *tableau)
  {
  int trame,avant;


  for (trame=l->debut;trame<l->fin;)
    {
    for (; trame<=l->fin && tableau[trame];trame++);
    for (avant=trame; trame<=l->fin && !tableau[trame];trame++);
    while  ( avant > l->debut && trame<l->fin && 
             (!tableau[trame] || !ecart_correct(tableau[avant-1],tableau[trame]) )
           ) trame++;
    if  (avant > l->debut && trame<l->fin && ecart_correct(tableau[avant-1],tableau[trame]) )
      {
      int debut,fin,i;

      debut = avant-1;
      fin   = trame;
      for (i=debut+1;i<fin;i++) tableau[i] = interpolation(debut,tableau[debut],fin,tableau[fin],i);
      }
    }

  }
/* *********************************************************************** */
static int point_accrochage(int debut,int fin,int *accrochage,int To_Moyenne)
{
#define POURCENTAGE 30
#define PROCHE_MOYENNE(e,bande)                                    \
			    (                                      \
			       ( (e) >= (To_Moyenne-bande) ) &&    \
			       ( (e) <= (To_Moyenne+bande) )       \
			    )
int bande,valeur,trame,pic;

 bande  = To_Moyenne * POURCENTAGE / 100;
 valeur = MAX_ENTIER;

 for (pic=0;pic<=1;pic++)
  for (trame=debut;trame<=fin;trame++)
    if ( abs(Coeff_Amdf[pic][trame].rang-To_Moyenne) < abs(valeur-To_Moyenne) )  
      {
      valeur = Coeff_Amdf[pic][trame].rang;
      *accrochage = trame;
      }


  if (PROCHE_MOYENNE(valeur,bande)) return valeur;
    
  return 0;

#undef POURCENTAGE
#undef PROCHE_MOYENNE
}


/* *********************************************************************** */
static int cherche_chemin(ZONE l,int To_moyen)
  {
  int accrochage;
   
  if ( !( l->ancrage=point_accrochage(l->debut,l->fin,&accrochage,To_moyen) ))
    return 0;

  extension_zone(accrochage,l->debut,l->fin,l->ancrage,Fo); 
  recupere_trou(l,Fo); 
  return 1;
  }

/* ************************************************************************ */
 
static void calcul_courbe_fo(int nb_trames,int *To_Moyen)
  {
  int   t,memo,debut;
  ZONE  l;
  

  /* on met tout a zero au debut */
  
  memo = *To_Moyen;

  for (debut=0; debut<nb_trames; debut++)  Fo[debut] = 0;
  
  for (l=zone;l;l=l->suiv)
    {
    if ( !cherche_chemin(l,*To_Moyen) )
      for (t=l->debut;t<=l->fin;t++) Fo[t]=0;
    else 
      {
#ifdef CORRECTION
      int cum=0,nb=0;

      for (t=l->fin; (t>= l->debut) ; t--)
	{
	cum += Fo[t];
	nb++;
	}
      *To_Moyen = ((2*cum) + (nb * memo)) / (3 * nb); 
      if (debug) printf("correction moyenne : %d\n",*To_Moyen);
#endif
      }
    }
  
  /* -------- on recalcule la moyenne sur le chemin choisi --------------*/
  
  min_fo = MAX_ENTIER;
  max_fo = 0;

  for ((*To_Moyen)=debut=t=0;t<nb_trames;t++)
    if (Fo[t]) 
      {
      (*To_Moyen) += Fo[t];
      Fo[t] = cst_freq_ech / Fo[t];
      if (max_fo < Fo[t]) max_fo = Fo[t];
      if (min_fo > Fo[t]) min_fo = Fo[t];
      debut++;
      }
  if (debut) (*To_Moyen) /= debut;

  if (debug) printf("MOYENNE FINALE : %d (fo=%d)\n",*To_Moyen, cst_freq_ech / *To_Moyen);
  }
  
/* ********************************************************************** */
static void calcul_fo_moyen(int nb_trames, int *To_Moyenne)
  {
  int  trame,nb,bulle;
  RESULT *table;
  int  To_Moyenne_Corrigee;

#define POURCENTAGE  30
#define ACCEPTABLE(valeur,moyenne,pourcentage)           \
     (                                                     \
	((valeur) >  (moyenne-pourcentage)) &&             \
	((valeur) <  (moyenne+pourcentage))                \
     )
#define TABLEAU(t) (abs(table[t].rang-(*To_Moyenne)))

  table = (RESULT *) ckalloc(sizeof(RESULT)*nb_trames);

  for (*To_Moyenne=trame=nb=0;trame<nb_trames;trame++)
    if ( VOISEE(trame) )
      {
       table[nb++] = Coeff_Amdf[0][trame];
      (*To_Moyenne) += Coeff_Amdf[0][trame].rang;
      }

  *To_Moyenne  = (nb)? ((*To_Moyenne) / nb) : 1;

  if (debug) printf("To moyen non corrige : %d (fo=%d) \n",*To_Moyenne,cst_freq_ech / *To_Moyenne);

  /* ------- correction de la valeur de fo ----------- */

  for (bulle=1;bulle;)
    {
    for (bulle=trame=0;trame<nb-1;trame++)
      if (TABLEAU(trame)>TABLEAU(trame+1))
	{
	RESULT temp;
	bulle          = 1;
	temp           = table[trame];
	table[trame]   = table[trame+1];
	table[trame+1] = temp;
	}
    }

  nb -= ((int ) POURCENTAGE * nb) / 100;

  for (To_Moyenne_Corrigee=0,trame=0;trame<nb;trame++)
   To_Moyenne_Corrigee += table[trame].rang;
    
  To_Moyenne_Corrigee = (nb)? (To_Moyenne_Corrigee / nb) : 1;

  /* -------- resultats ----------------------------------- */

  *To_Moyenne = To_Moyenne_Corrigee;  

  if (debug) printf("moyenne (a %d%% presque partout): %d (fo=%d)\n",100-POURCENTAGE, *To_Moyenne,cst_freq_ech / *To_Moyenne);

  ckfree((char *) table);

#undef POURCENTAGE
#undef ACCEPTABLE
#undef TABLEAU
  }

/* ************************************************************************ */

static ZONE calcul_zones_voisees(int nb_trames)
  {
  int trame,debut;
  ZONE z = NULL;
  

  for (trame=0;trame<nb_trames;)
    {
    while ( (trame<nb_trames) && (NON_VOISEE(trame)) ) trame++;
    for (debut = trame; trame<nb_trames && VOISEE(trame);trame++ );
    
    if (trame<=nb_trames  && debut<trame)
      {
      ZONE t,l,pred;
      
      t = (ZONE) ckalloc(sizeof(struct bidon));
      t->debut   =   debut;
      t->fin     =   trame-1;
      t->ancrage =   0;
      t->suiv    =   NULL;
      
      for (l=z,pred=NULL; l;pred=l,l=l->suiv);
      
      t->pred    = pred;
      if (pred) pred->suiv = t;
                      else z=t;
      }
    }
  
  return z;  
  }
/* *********************************************************************** */
static int calcul_nrj_dpz(Sound *s, Tcl_Interp *interp, int start,int longueur)
{
  int J,JJ,m,i,j,trame,dpz,sens;
  double nrj;
  
  max_nrj = max_dpz = 0;
  min_nrj = min_dpz = MAX_ENTIER;

  Snack_ProgressCallback(s->cmdPtr, interp, "Computing pitch", 0.0);

  for (trame=i=0; i<longueur; i += cst_step_hamming,trame++) {
    J = minimum(s->length,(i+cst_length_hamming));
    JJ = J-1;
    if (s->length < i + start + cst_length_hamming) {
      Snack_GetSoundData(s, i+start, Signal, s->length - i + start);
      for (j = s->length - i + start; j < cst_length_hamming; j++) {
	Signal[j] = 0.0f;
      }
    } else {
      Snack_GetSoundData(s, i+start, Signal, cst_length_hamming);
    }    

    /* ---- nrj ---- */
    for (nrj=0.0,j=0; j<J-i; j++) 
      nrj += CARRE((double) Signal[j]);  
    
    m = Nrj[trame] = (short) (10 * log10(nrj));
    
    if (m > max_nrj) max_nrj = m;
    if (m < min_nrj) min_nrj = m;
    
    /* ---- dpz ---- */
    for (dpz=0,j=0; j<J-i; j++) {
      while ( (j<J-i) && (abs((int)Signal[j]) > EPSILON) ) j++; /* looking just close to zero values */
      if (j<J-i) dpz ++;
      sens = ( ((j-1) >= 0) && (Signal[j-1] > Signal[j])); 
      if (sens) while ( (j<JJ-i) && (Signal[j] > Signal[j+1]) ) j++;
      else while ( (j<JJ-i) && (Signal[j] <= Signal[j+1]) ) j++;      
    }
    m = Dpz[trame] = dpz;
    
    if (m > max_dpz) max_dpz = m;
    if (m < min_dpz) min_dpz = m;

    if ((trame % 300) == 299) {
      int res = Snack_ProgressCallback(s->cmdPtr, interp, "Computing pitch",
				       0.05 * (double) i / longueur);
      if (res != TCL_OK) {
	return TCL_ERROR;
      }
    }  
  }

  seuil_nrj = min_nrj + (SEUIL_NRJ*(max_nrj-min_nrj))/100;   
  seuil_dpz = min_dpz + (SEUIL_DPZ*(max_dpz-min_dpz))/100;  
 
  if (debug) printf("dpz <%d,%d>, nrj <%d,%d> => Seuil nrj: %d, Seuil dpz: %d\n",min_dpz,max_dpz,min_nrj,max_nrj,seuil_nrj,seuil_dpz);

  return trame;
  }

/* ********************************************************************** */
/*
static void adjust_signal(int longueur, int freq, int debug)
{
long i,moy,m;

for (i=0; i<(m=minimum(freq,longueur)); moy += Signal[i++]);
moy /= (m)? m:1;
if (debug) fprintf(stderr,"ajustement de %d dans tout le signal\n",moy);
if (moy) for (i=0; i<longueur; Signal[i++] -= (short) moy);
}
*/
/* ********************************************************************** */

int
pitchCmd(Sound *s, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
  int longueur, nb_trames, nb_trames_alloc, To_Moyen;
  int *Hammer;
  int i;
  int fmin = 60, fmax = 400, lquick = 1/*, adjust = 0*/, nbframes;
  short minnrj, maxnrj, minfo, maxfo, mindpz, maxdpz;
  int arg, startpos = 0, endpos = -1, result, start;
  Tcl_Obj *list;
  static CONST84 char *subOptionStrings[] = {
    "-start", "-end", "-maxpitch", "-minpitch", "-progress",
    "-framelength", "-method", "-windowlength", NULL
  };
  enum subOptions {
    START, END, F0MAX, F0MIN, PROGRESS, FRAME, METHOD, WINLEN
  };

  if (s->debug > 0) { Snack_WriteLog("Enter pitchCmd\n"); }

  if (s->nchannels != 1) {
    Tcl_AppendResult(interp, "pitch only works with Mono sounds",
		     (char *) NULL);
    return TCL_ERROR;
  }

  for (arg = 2; arg < objc; arg += 2) {
    char *opt = Tcl_GetStringFromObj(objv[arg], NULL);
    char *val = Tcl_GetStringFromObj(objv[arg+1], NULL);

    if ((strcmp("-method", opt) == 0) && (strcasecmp("esps", val) == 0)) {
      Get_f0(s, interp, objc, objv);
      return TCL_OK;
    }
  }

  if (s->cmdPtr != NULL) {
    Tcl_DecrRefCount(s->cmdPtr);
    s->cmdPtr = NULL;
  }

  for (arg = 2; arg < objc; arg += 2) {
    int index;
	
    if (Tcl_GetIndexFromObj(interp, objv[arg], subOptionStrings,
			    "option", 0, &index) != TCL_OK) {
      return TCL_ERROR;
    }

    if (arg + 1 == objc) {
      Tcl_AppendResult(interp, "No argument given for ",
		       subOptionStrings[index], " option", (char *) NULL);
      return TCL_ERROR;
    }
    
    switch ((enum subOptions) index) {
    case START:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &startpos) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case END:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &endpos) != TCL_OK)
	  return TCL_ERROR;
	break;
      }
    case F0MAX:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &fmax) != TCL_OK)
	  return TCL_ERROR;
	if (fmax <= 50) {
	  Tcl_AppendResult(interp, "Maximum pitch must be > 50", NULL);
	  return TCL_ERROR;
	}
	break;
      }
    case F0MIN:
      {
	if (Tcl_GetIntFromObj(interp, objv[arg+1], &fmin) != TCL_OK)
	  return TCL_ERROR;
	if (fmin <= 50) {
	  Tcl_AppendResult(interp, "Minimum pitch must be > 50", NULL);
	  return TCL_ERROR;
	}
	break;
      }
    case PROGRESS:
      {
	char *str = Tcl_GetStringFromObj(objv[arg+1], NULL);
	
	if (strlen(str) > 0) {
	  Tcl_IncrRefCount(objv[arg+1]);
	  s->cmdPtr = objv[arg+1];
	}
	break;
      }
    case FRAME:
      {
	break;
      }
    case METHOD:
      {
	break;
      }
    case WINLEN:
      {
	break;
      }
    }
  }
  if (fmax <= fmin) {
    Tcl_AppendResult(interp, "maxpitch must be > minpitch", NULL);
    return TCL_ERROR;
  }
  if (startpos < 0) startpos = 0;
  if (endpos >= (s->length - 1) || endpos == -1)
    endpos = s->length - 1;
  if (startpos > endpos) return TCL_OK;

  quick = lquick;
  init(s->samprate, fmin, fmax);

  start = startpos - (cst_length_hamming / 2);
  if (start < 0) start = 0;
  if (endpos - start + 1 < cst_length_hamming) {
    endpos = cst_length_hamming + start - 1;
    if (endpos >= s->length) return TCL_OK;
  }
  longueur = endpos - start + 1;
  
  if ((Signal = (float *) ckalloc(cst_length_hamming * sizeof(float))) == NULL) {
    Tcl_AppendResult(interp, "Couldn't allocate buffer!", NULL);
    return TCL_ERROR;
  }

  /*  if (adjust) adjust_signal(longueur,s->samprate,s->debug);*/

  nb_trames = (longueur / cst_step_hamming) + 10;
  Nrj =  (short *) ckalloc(sizeof(short) * nb_trames);   
  Dpz =  (short *) ckalloc(sizeof(short) * nb_trames);
  Vois = (short *) ckalloc(sizeof(short) * nb_trames);
  Fo =   (short *) ckalloc(sizeof(short) * nb_trames);
  Resultat = (int **) ckalloc(sizeof(int *) * nb_trames);

  for (i = 0; i < nb_trames; i++) {
    Resultat[i] = (int *) ckalloc(sizeof(int) * (cst_step_max-cst_step_min+1));
  }
  nb_trames_alloc = nb_trames;
  nb_trames = nbframes = calcul_nrj_dpz(s, interp, start, longueur);

  Hamming = (double *) ckalloc(sizeof(double)*cst_length_hamming);
  Hammer = (int *) ckalloc(sizeof(int) * cst_length_hamming);
  for (i=0;i<cst_pics_amdf;i++) {
    Coeff_Amdf[i] = (RESULT *) ckalloc(sizeof(RESULT)* nb_trames);
  }

  precalcul_hamming();

  result = parametre_amdf(s, interp, start, longueur, (int *)&(nbframes),
			  Hammer); 

  if (result == TCL_OK) {
    if (debug) printf("nbframes=%d\n",nbframes);
    calcul_voisement(nbframes); 
    zone = calcul_zones_voisees(nbframes); 
    calcul_fo_moyen(nbframes,&To_Moyen);    
    calcul_courbe_fo(nbframes,&To_Moyen);    

    minfo = (short) min_fo;  
    maxfo = (short) max_fo;
    minnrj = (short) min_nrj;
    maxnrj = (short) max_nrj;
    mindpz = (short) min_dpz;
    maxdpz = (short) max_dpz;

    if (debug && quick) 
      printf("%d trames coupees sur %d -> %d %% (seuil nrj = %d, seuil dpz = %d) \n",
	     nb_coupe,nbframes,POURCENT(nb_coupe,nbframes),seuil_nrj,seuil_dpz);
    libere_zone(zone);
    for (i=0;i<nb_trames_alloc;i++) 
      if (Resultat[i]) ckfree((char *) Resultat[i]);
  }
  ckfree((char *) Hamming);
  ckfree((char *) Hammer);
  ckfree((char *)Signal);
  libere_coeff_amdf(); 
  ckfree((char *) Resultat);
  if (result == TCL_OK) {
    int n = cst_length_hamming / (2 * cst_step_hamming) - startpos / cst_step_hamming;

    list = Tcl_NewListObj(0, NULL);
    for (i = 0; i < n; i++) {
      Tcl_ListObjAppendElement(interp, list, Tcl_NewDoubleObj(0.0));
    }
    for (i = 0; i < nbframes; i++) {
      Tcl_ListObjAppendElement(interp, list, Tcl_NewDoubleObj((double)Fo[i]));
    }
    Tcl_SetObjResult(interp, list);
  }
  ckfree((char *) Nrj);
  ckfree((char *) Dpz);
  ckfree((char *) Vois);
  ckfree((char *) Fo);

  if (s->debug > 0) { Snack_WriteLog("Exit pitchCmd\n"); }

  return TCL_OK;
}

int
cPitch(Sound *s, Tcl_Interp *interp, int **outlist, int *length)
{
  int longueur, nb_trames, To_Moyen;
  int *Hammer;
  int i;
  int fmin = 60, fmax = 400, lquick = 1/*, adjust = 0*/, nbframes;
  short minnrj, maxnrj, minfo, maxfo, mindpz, maxdpz;
  int startpos = 0, endpos = -1, result, start;

  if (s->debug > 0) { Snack_WriteLog("Enter pitchCmd\n"); }

  if (fmax <= fmin) {
    Tcl_AppendResult(interp, "maxpitch must be > minpitch", NULL);
    return TCL_ERROR;
  }
  if (startpos < 0) startpos = 0;
  if (endpos >= (s->length - 1) || endpos == -1)
    endpos = s->length - 1;
  if (startpos > endpos) return TCL_OK;

  quick = lquick;
  init(s->samprate, fmin, fmax);

  start = startpos - (cst_length_hamming / 2);
  if (start < 0) start = 0;
  longueur = endpos - start + 1;

  if ((Signal = (float *) ckalloc(cst_length_hamming * sizeof(float))) == NULL) {
    Tcl_AppendResult(interp, "Couldn't allocate buffer!", NULL);
    return TCL_ERROR;
  }

  nb_trames = (longueur / cst_step_hamming) + 10;
  Nrj =  (short *) ckalloc(sizeof(short) * nb_trames);   
  Dpz =  (short *) ckalloc(sizeof(short) * nb_trames);
  Vois = (short *) ckalloc(sizeof(short) * nb_trames);
  Fo =   (short *) ckalloc(sizeof(short) * nb_trames);
  Resultat = (int **) ckalloc(sizeof(int *) * nb_trames);

  for (i = 0; i < nb_trames; i++) {
    Resultat[i] = (int *) ckalloc(sizeof(int) * (cst_step_max-cst_step_min+1));
  }

  nb_trames = nbframes = calcul_nrj_dpz(s, interp, start, longueur);

  Hamming = (double *) ckalloc(sizeof(double)*cst_length_hamming);
  Hammer = (int *) ckalloc(sizeof(int) * cst_length_hamming);
  for (i=0;i<cst_pics_amdf;i++) {
    Coeff_Amdf[i] = (RESULT *) ckalloc(sizeof(RESULT)* nb_trames);
  }

  precalcul_hamming();

  result = parametre_amdf(s, interp, start, longueur, (int *)&(nbframes),
			  Hammer); 

  if (result == TCL_OK) {
    calcul_voisement(nbframes); 
    zone = calcul_zones_voisees(nbframes); 
    calcul_fo_moyen(nbframes,&To_Moyen);    
    calcul_courbe_fo(nbframes,&To_Moyen);    

    minfo = (short) min_fo;  
    maxfo = (short) max_fo;
    minnrj = (short) min_nrj;
    maxnrj = (short) max_nrj;
    mindpz = (short) min_dpz;
    maxdpz = (short) max_dpz;
  
    libere_zone(zone);
    for (i=0;i<nbframes;i++) 
      if (Resultat[i]) ckfree((char *) Resultat[i]);
  }
  ckfree((char *) Hamming);
  ckfree((char *) Hammer);
  ckfree((char *)Signal);
  libere_coeff_amdf(); 
  ckfree((char *) Resultat);
  if (result == TCL_OK) {
    int n = cst_length_hamming / (2 * cst_step_hamming) - startpos / cst_step_hamming;
    int *tmp = (int *)ckalloc(sizeof(int) * (n + nb_trames));
    for (i = 0; i < n; i++) {
      tmp[i] = 0;
    }
    for (i = n; i < nbframes + n; i++) {
      tmp[i] = Fo[i-n];
    }
    *outlist = tmp;
    *length = nbframes + n;
  }
  ckfree((char *) Nrj);
  ckfree((char *) Dpz);
  ckfree((char *) Vois);
  ckfree((char *) Fo);

  if (s->debug > 0) { Snack_WriteLog("Exit pitchCmd\n"); }

  return TCL_OK;
}
