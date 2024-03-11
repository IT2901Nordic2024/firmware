#ifndef _APP_VERSION_H_
#define _APP_VERSION_H_

/*  values come from cmake/version.cmake
 * BUILD_VERSION related  values will be 'git describe',
 * alternatively user defined BUILD_VERSION.
 */

/* #undef ZEPHYR_VERSION_CODE */
/* #undef ZEPHYR_VERSION */

#define APPVERSION          0x10B6300
#define APP_VERSION_NUMBER  0x10B63
#define APP_VERSION_MAJOR   1
#define APP_VERSION_MINOR   11
#define APP_PATCHLEVEL      99
#define APP_VERSION_STRING  "1.11.99-dev"

#define APP_BUILD_VERSION edfe1e1465db


#endif /* _APP_VERSION_H_ */
