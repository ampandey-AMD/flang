!
! Copyright (c) 2019, Advanced Micro Devices, Inc. All rights reserved.
!
! F2008 Compliance Tests: Bit-intrinsics test cases
!
! Date of Modification: Tue Feb 11 20:45:41 IST 2020
! Purpose: Test the operation of MASKL intrinsic
!
PROGRAM BITINT23
  IMPLICIT NONE
  INTEGER(1) :: SIZE, RESULT
  INTEGER, PARAMETER :: N = 1
  LOGICAL EXP(N), RES(N)

  SIZE = 4

  RESULT = MASKL(SIZE, 1)

  PRINT *, '------'
  PRINT *, RESULT
  PRINT '(B8.8)', RESULT
  WRITE(UNIT=*,FMT="(B32.32)") RESULT
  IF (RESULT /= INT(B'11110000', 1)) THEN
    STOP "FAIL"
  ELSE
    PRINT *, "PASS"
  END IF
  CALL CHECK(RES, EXP, N)
END PROGRAM
