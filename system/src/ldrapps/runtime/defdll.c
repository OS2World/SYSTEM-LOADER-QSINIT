
extern unsigned __stdcall _wc_static_init();
extern unsigned __stdcall _wc_static_fini();

unsigned __stdcall LibMain(unsigned hmod, unsigned termination) 
{
   return termination?_wc_static_fini():_wc_static_init();
}
