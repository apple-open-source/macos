/* APPLE LOCAL funtion-ptr returning complex type */
/* { dg-do compile { target powerpc*-*-* } } */
/* { dg-options "-mpowerpc64" } */

typedef double _Complex t_complexe;


extern t_complexe *cree_tabnum_compl(unsigned int);

t_complexe *func1(t_complexe *pSrc1, t_complexe *pDst1,
                   unsigned int iTaille,
                  t_complexe (*p_pFunc)(t_complexe))
{
 unsigned int iCpt;
 t_complexe *pzRes;
 t_complexe *pzSrc;

 iCpt = iTaille;
 pzSrc = pSrc1;
 pzRes = pDst1;

 while(iCpt!=0)
 {
  *pzRes++=p_pFunc(*pzSrc++);
  iCpt--;
 }
 return pzRes;
}
