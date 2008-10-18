*DECK GAMGEN
      SUBROUTINE GAMGEN
      IMPLICIT REAL*8(A-H,O-Z)
      COMMON/TABLE/C(1200,6)
      DIMENSION Y(410),F(9)
      DATA PT15/0.15D0/,PT05/0.05D0/,PT184/0.184D0/,SIX/6.0D0/,
     $     FOUR/4.0D0/
C
C     INITIALIZE THE F(M,T) ROUTINE.
      CALL FMTSET(0,0,0)
C
C     GENERATE THE DESIRED F(M,T) FOR THE COMPLETE RANGE.
C     WE WILL COMPUTE F(0,T) THROUGH F(4,T) IN THIS SECTION.
      T=-PT15
      DO 10 I=1,404
      T=T+PT05
      CALL FMTGEN(F,T,6,ICK)
C     COPY THE RETURNED VALUES INTO PLACES WHERE THEY CAN BE
C     REACHED LATER.
      C(I,2)=F(1)
      C(I,3)=F(2)
      C(I,4)=F(3)
      C(I,5)=F(4)
      C(I,6)=F(5)
   10 Y(I  )=F(6)
C
C     COMPUTE THE INTERPOLATION TABLE WITH THE VALUES AVAILABLE.
C     THIS IS SOMEWHAT COMPLICATED BY THE FACT THAT SOME OF THE
C     INTEGRALS ARE IN Y RATHER THAN IN C.
      DO 40 K=1,6
      DO 40 I=1,400
      J=I+2
C     K INDEXES THE VALUE OF M IN F(M,T).
C     I INDEXES THE INTERPOLATION TABLE (C).
C     J INDEXES THE STORED VALUES OF F(M,T).
C     IF K .EQ. 5, WE MUST USE ALTERNATE CODE BECAUSE THE INTEGRALS
C     ARE STORED IN Y.
      IF(K-6)20,30,20
C     INTEGRALS IN C, PROCEED AS NORMAL.
   20 TEMP1=C(J+1,K+1)+C(J-1,K+1)-(C(J,K+1)+C(J,K+1))
      TEMP2=SIX*C(J,K+1)-FOUR*(C(J+1,K+1)+C(J-1,K+1))+C(J-2,K+1)+C(J+2,K
     $+1)
      C(I    ,K)=C(J,K+1)
      C(I+400,K)=C(J+1,K+1)-C(J,K+1)
      C(I+800,K)=(TEMP1-PT184*TEMP2)/SIX
      GO TO 40
C     ALTERNATE CODE USING Y.
   30 TEMP1=Y(J+1)+Y(J-1)-(Y(J)+Y(J))
      TEMP2=SIX*Y(J)-FOUR*(Y(J+1)+Y(J-1))+Y(J-2)+Y(J+2)
      C(I    ,K)=Y(J)
      C(I+400,K)=Y(J+1)-Y(J)
      C(I+800,K)=(TEMP1-PT184*TEMP2)/SIX
   40 CONTINUE
      RETURN
      END
