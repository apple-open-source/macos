/* { dg-do compile } */
/* { dg-options "-floop-transpose" } */
double ptr_[432];
void symddm_(double *ddm_, int nat_)
{
   int l_, m_;
   double Tmp20, v_[9];
   for(l_ = 1;l_ <= 3;l_++) {
     Tmp20 = 0.;
     for(m_ = 1;m_ <= 2;m_ += 2) {
         Tmp20 = Tmp20 + ddm_[3*((m_ + 1))]* ptr_[l_ + 3*((m_ + 1))];
       }
     Tmp20 = Tmp20 + ddm_[3*(m_)]* ptr_[l_ + 3*(m_)];
     v_[3*(l_)] = Tmp20;
   }
}
