!
! Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.
!
! Date of Modification: Sep 10 2019
!
! F2008 Compliance Tests: Stop code - Execution control
!
! This program checks if the implementation supports a STOP code that is also returned as the program exit code which is usually 32 bits
!
PROGRAM scode02
	IMPLICIT NONE
  INTEGER, PARAMETER :: N = 1
  LOGICAL exp(N), res(N)
	CALL check(res, exp, N)

	CALL scode_i
END PROGRAM

IMPURE SUBROUTINE scode_i
	IMPLICIT NONE
	INTEGER code

	code = 9999
	STOP code
END SUBROUTINE
