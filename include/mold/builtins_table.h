#ifndef BUILTIN
#define BUILTIN(...)
#endif
BUILTIN("Int.max",
  PARAMS(TY(INT, true), TY(INT, true)),
  TY(INT, true), true,
  builtin_int_max,
  "Returns the larger parameter for integers")
BUILTIN("Real.max",
  PARAMS(TY(REAL, true), TY(REAL, true)),
  TY(REAL, true),
  true,
  builtin_real_max,
  "Returns the larger parameter for real numbers")
BUILTIN("String.length",
  PARAMS(TY(STRING, true)),
  PARAMS(TY(INT, false)),
  true,
  builtin_str_length,
  "Returns the length of a string")
BUILTIN("Date.now",
  PARAMS(),
  TY(DATE, false),
  false,
  builtin_date_now,
  "Returns the current date")
BUILTIN("not",
  PARAMS(TY(BOOL, true)),
  TY(BOOL, true),
  true,
  builtin_not,
  "Reverses a boolean value")
