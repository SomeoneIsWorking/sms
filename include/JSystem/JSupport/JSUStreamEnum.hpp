#ifndef JSU_STREAM_ENUM_H
#define JSU_STREAM_ENUM_H

enum JSUStreamSeekFrom {
	JSUStreamSeekFrom_SET = 0,
	JSUStreamSeekFrom_CUR = 1,
	JSUStreamSeekFrom_END = 2
};

// glibc <stdio.h> #defines EOF as (-1), which would macro-expand the enum
// value below. The GC SDK stdio was header-shimmed for Path B; under Path A
// (Aurora) we use glibc's stdio, so #undef EOF here to let the decomp keep
// using EOF as an enumerator.
#undef EOF
enum EIoState { GOOD = 0, EOF = 1 };

#endif
