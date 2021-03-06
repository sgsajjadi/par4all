/* Check the interprocedural summary precondition computation when a
   module source code is generated by initializer.

   Note that the information i==1 is not propagated to foo's
   precondition because foo is synthesized without any effect on its
   parameters f1 and f2. The information is not lost but filtered out.
*/

int generate01()
{
  int i = 1;
  float x = 0.5;

  return foo(i, x);
}
