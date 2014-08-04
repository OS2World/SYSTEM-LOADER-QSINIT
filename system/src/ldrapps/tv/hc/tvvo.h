/*
        Define __DOS16__ if it is not defined
*/
#if defined(__MSDOS__) && !defined(__DOS32__) && !defined(__DOS16__)
#define __DOS16__
#endif
/*
        Check target platform
*/
#if !defined(__MSDOS__) && !defined(__OS2__) && !defined(__NT__) && !defined(__QSINIT__)
#error "Unknown target platform!"
#endif
/*
        Define TV_CDECL
*/
#if defined(__NT__) || defined(__WATCOMC__)
#define TV_CDECL
#elif defined(__IBMCPP__)
#define TV_CDECL      __cdecl
#else
#define TV_CDECL        cdecl
#endif

#if defined(_MSC_VER)
#pragma warning(disable:4099)
#endif

/*
        Issue some pragmas for Borland
*/
#if defined(__MSDOS__) && defined(__BORLANDC__)
#pragma option -Vo-
#if defined( __BCOPT__ )
#pragma option -po-
#endif
#endif // __MSDOS__ && __BORLANDC__
