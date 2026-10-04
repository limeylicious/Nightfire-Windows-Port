#ifndef DRIVING_PUBLICATION324_H
#define DRIVING_PUBLICATION324_H
#include "driving_lease322.h"
/* Cooperative kernel-read publication, not general target ownership.
 * Only original two-join publication is admitted. Unknown pointer/alias
 * escapes cannot be made exclusive by this protocol; never extend handoffs. */
int xbox_Publication324Begin(DL322Lease *lease);
int xbox_Publication324End(DL322Lease *lease);
BOOL xbox_PublicationRead324(HANDLE process,LPCVOID source,LPVOID out,SIZE_T bytes,SIZE_T *got);
void xbox_Publication324Report(void);
#ifdef DRIVING_COMMAND_PUBLISH325
/* Scope only the scalar load, inside its existing permission read lock.
 * The returned token records whether this thread acquired shared ownership. */
int xbox_CommandPublication325Begin(void);
void xbox_CommandPublication325End(int shared);
#endif
#endif
