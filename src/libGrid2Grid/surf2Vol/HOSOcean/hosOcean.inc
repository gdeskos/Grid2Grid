Subroutine initHOSOceanSurf2VolDict(this, HOSDict)
    use mfpGeneral, only : isStringEqual
    implicit none
    class(typHOSOcean),intent(inout) :: this
    Type(typDictionaryPtr), intent(in)  :: HOSdict

    real(rp) :: zMin, zMax, zMinRatio, zMaxRatio
    integer  :: nZmin, nZmax

    Character(len=CHAR_LEN) :: zMeshType, HOSfileType

    if (this%isInitialized_) return

    this%dict_ = HOSDict

    !! - Set file name and file unit
    this%hosFile_%name = HOSdict%getChar("filePath")
    this%hosFile_%unit = callFileUnit()

    this%hosProcedure_ = HOSdict%getCharOrDefault("procedure", "velocity")

    HOSfileType = HOSdict%getCharOrDefault("fileType", "NONE")

    if (isStringEqual(HOSfileType,"NONE")) then
        !! - Set output format
        if (index(trim(this%hosFile_%name), ".dat", .true.) /= 0) then
          this%isHDF5Format_ = .false.
        else if (index(trim(this%hosFile_%name), ".h5", .true.) /= 0 .or. &
          index(trim(this%hosFile_%name), ".hdf5", .true.) /= 0 ) then

#ifdef ENABLE_HDF5
            this%isHDF5Format_ = .true.
#else
            write(*,*) " NO HDF library is supported. Please Compile with HDF5 Option."
            stop
#endif

        else
          write(error_unit, '(3A)') "Error in init_read_mod: Unknown file extension for '",&
            trim(this%hosFile_%name), "' (should be .dat, .h5, or .hdf5)"
          stop
        endif
    else
        if (isStringEqual(HOSfileType, "ASCII")) then
            this%isHDF5Format_ = .false.
        else if (isStringEqual(HOSfileType, "HDF5")) then

#ifdef ENABLE_HDF5
            this%isHDF5Format_ = .true.
#else
            write(*,*) " NO HDF library is supported. Please Compile with HDF5 Option."
            stop
#endif

        else
            write(*,*) "    [ERROR] HOS Dictionary : wrong fileType is given."
            write(*,*) "    fileType : ", trim(HOSfileType)
            write(*,*) " "
            stop
        end if
    end if

    zMin    = HOSdict%getReal("zMin")
    zMax    = HOSdict%getReal("zMax")

    nZmin    = HOSdict%getInt("nZMin")
    nZmax    = HOSdict%getInt("nZMax")

    zMeshType = HOSdict%getChar("zMeshType")

    zMinRatio = 1.0_RP
    zMaxRatio = 1.0_RP
    if (isStringEqual(zMeshType, "meshRatio")) then
        zMinRatio    = HOSdict%getRealOrDefault("zMinRatio", 3.0_RP)
        zMaxRatio    = HOSdict%getRealOrDefault("zMaxRatio", 3.0_RP)
    end if

    ! Read and initalize HOS simulation parameter
    Call this%init_read_mod()

    ! Build Mesh and Calculate Wave Numbers
    Call this%buildGlobalMesh(zMin, zMax, nZmin, nZmax, zMinRatio, zMaxRatio)

    ! Initialize Fourier transforms (FFTW)
    Call this%hosOceanFFTW_%initialize(this%nXmode_, this%nYmode_)

    ! Read HOS Ocean mode for first time step
    Call this%correct(0)

    this%isInitialized_ = .TRUE.

End Subroutine

!!!- initialize HOS Ocean reading Module
SUBROUTINE initHOSOcean(this, fileName, zMin, zMax, nZmin, nZmax, zMinRatio, zMaxRatio)
    !---------------------------------------------------------
    !   Input :
    !       fileName : file path of HOS Ocean mode file
    !       zMin     : Z min
    !       zMax     : Z max
    !       nZmin    : number of HOS z-directional mesh ( z < 0 )
    !       nZmax    : number of HOS z-directional mesh ( z > 0 )
    !---------------------------------------------------------
    implicit none
    class(typHOSOcean), intent(inout) :: this
    character(len=*),intent(in) :: fileName
    real(rp),intent(in) :: zMin, zMax
    integer, intent(in) :: nZmin, nZmax
    real(rp), optional  :: zMinRatio, zMaxRatio
    !!!......................................

    if (this%isInitialized_) return
    !! - Set HOS volume procedure
    this%hosProcedure_ = "velocity"

    !! - Set file name and file unit
    this%hosFile_%name = fileName
    this%hosFile_%unit = callFileUnit()

    !! - Set output format
    if (index(trim(this%hosFile_%name), ".dat", .true.) /= 0) then
      this%isHDF5Format_ = .false.

    else if (index(trim(this%hosFile_%name), ".h5", .true.) /= 0 .or. &
      index(trim(this%hosFile_%name), ".hdf5", .true.) /= 0 ) then
#ifdef ENABLE_HDF5
        this%isHDF5Format_ = .true.
#else
        write(*,*) " NO HDF library is supported. Please Compile with HDF5 Option."
        stop
#endif

    else
      write(error_unit, '(3A)') "Error in init_read_mod: Unknown file extension for '",&
        trim(this%hosFile_%name), "' (should be .dat, .h5, or .hdf5)"
      stop
    endif

    ! Read and initalize HOS simulation parameter
    Call this%init_read_mod()

    ! Build Mesh and Calculate Wave Numbers
    Call this%buildGlobalMesh(zMin, zMax, nZmin, nZmax, zMinRatio, zMaxRatio)

    ! Initialize Fourier transforms (FFTW)
    Call this%hosOceanFFTW_%initialize(this%nXmode_, this%nYmode_)

    ! Read HOS Ocean mode for first time step
    Call this%correct(0)

    this%isInitialized_ = .TRUE.

END SUBROUTINE

SUBROUTINE correctHOSOcean(this, iTime)
    Implicit none

    class(typHOSOcean), intent(inout) :: this
    Integer, intent(in) :: iTime

    !! - Check time index
    if (iTime.eq.this%iReadTimeIndex_ ) return

    !! - Update time index
    this%iReadTimeIndex_ = iTime

    !! - Read HOS NWT mode for given time index
    Call this%read_mod(iTime)

    !! - Construct General Mode Flow quantities by FFT
    Call this%reconstructionFFTs()

    Call this%hosMesh_%calculDimValue()

END SUBROUTINE

SUBROUTINE destroyHOSOcean(this)
    Implicit none
    class(typHOSOcean), intent(inout) :: this
    this%isInitialized_ = .FALSE.
    Call this%hosMode_%destroy()
    Call this%hosMesh_%destroyOceanMesh()
    Call this%hosOceanFFTW_%destroy()
END SUBROUTINE

!!!- Initialize data from volumic mode description generated by HOS-Ocean
subroutine init_read_mod_HOSOcean(this)
    implicit none
    class(typHOSOcean), intent(inout) :: this
    REAL(RP) :: x1, x2
    !!!......................................

    if (this%isHDF5Format_) then
#ifdef ENABLE_HDF5
      Call this%init_hdf5_read_mod(x1, x2)
#else
      write(*,*) " NO HDF library is supported. Please Compile with HDF5 Option."
      stop
#endif
    else
      Call this%init_ascii_read_mod(x1, x2)
    endif

    this%nXmodeOrg_  = nint(x1)

    this%nXo2p1Org_ = this%nXmodeOrg_ / 2 + 1

    ! Set Simulation Parameters
    this%nXmode_   = nint(x1) * this%dict_%getIntOrDefault("extraInterpolationNumber", 1)

    !! Consider 2D Case
    this%nYmodeOrg_  = nint(x2)
    if (x2.eq.1) then
      this%nYmode_     = nint(x2)
    else
      this%nYmode_   = nint(x2) * this%dict_%getIntOrDefault("extraInterpolationNumber", 1)
    end if
    this%nYo2p1Org_ = this%nYmodeOrg_ / 2 + 1

    this%nHOSTime_ = nint(this%Tstop_ / this%dtOut_)

    this%Tstop_    = this%Tstop_ * this%dimT_
    this%dtOut_    = this%dtOut_ * this%dimT_

    this%nXo2p1_   = this%nXmode_ / 2 + 1
    this%nYo2p1_   = this%nYmode_ / 2 + 1

    this%isXeven_ = (MOD(this%nXmode_,2) == 0)
    this%isYeven_ = (MOD(this%nYmode_,2) == 0)

    this%xLen_  = this%nonDimxLen_ * this%dimL_
    this%yLen_  = this%nonDimyLen_ * this%dimL_
    this%depth_ = this%nonDimDepth_ * this%dimL_

    ! Reading Time Index
    this%iReadTimeIndex_ = -1

    ! set size of HOS Ocean mode array
    Call this%hosMode_%allocOceanMode(this%nXo2p1_, this%nYmode_, this%hosProcedure_)

end subroutine

subroutine init_ascii_read_mod_HOSOcean(this, x1, x2)
    implicit none
    class(typHOSOcean), intent(inout) :: this
    REAL(RP), intent(inout) :: x1, x2

    !!!......................................
    ! We will look at first ten variables written on 18 characters
    OPEN(unit = this%hosFile_%unit, &
         file = this%hosFile_%name, &
         status='OLD', &
         form='FORMATTED', &
         access='DIRECT', &
         recl=18*10)

    ! Read HOS Ocean simulation parameters
    READ(this%hosFile_%unit,'(10(ES17.10,1X))',rec=1) x1, x2, &
                                                this%dtOut_, this%Tstop_, &
                                                this%nonDimxLen_, this%nonDimyLen_, this%nonDimDepth_, &
                                                this%gravi, this%dimL_, this%dimT_

    CLOSE(this%hosFile_%unit) ! close HOS Ocean file

    IF ( abs(this%dimT_ - 0.0_RP) < tiny ) then
        write(*,*) "[ERROR] typHOSOcean::init_read_mod()"
        write(*,*) " "
        write(*,*) "    Input file is not HOS Ocean Result File."
        write(*,*) " "
        stop
    ENDIF

end subroutine

#ifdef ENABLE_HDF5
subroutine init_hdf5_read_mod_mod_HOSOcean(this, x1, x2)
     implicit none
     class(typHOSOcean), intent(inout) :: this
     REAL(RP), intent(inout) :: x1, x2
     !!!......................................

     CHARACTER(len=6), parameter :: HEADER_DSET_NAME = "header"
     INTEGER, parameter :: HEADER_SIZE = 10
     INTEGER(hsize_t), dimension(1), parameter :: HEADER_DATA_DIMS = (/HEADER_SIZE/)
     REAL(DP), dimension(HEADER_SIZE) :: header_data
     INTEGER(hid_t) :: file_id
     INTEGER(hid_t) :: header_dset_id
     INTEGER :: error
     CHARACTER(len=HOS_TYPE_NAME_SIZE) :: hos_type_name

     ! Open FORTRAN interface
     Call h5open_f(error)

     ! ! Open the file
     Call h5fopen_f(this%hosFile_%name, H5F_ACC_RDONLY_F, file_id, error)

     hos_type_name = trim(read_hos_type_name_mod(file_id))
     if (hos_type_name /= "HOS_Ocean") then
       write(*,*) "[ERROR] typHOSOcean::init_hdf5_read_mod()"
       write(*,*) " "
       write(*,*) "    Input file is not HOS Ocean Result File but '", trim(hos_type_name),"'"
       write(*,*) " "
       stop
     endif

     ! ! ! Open the header dataset
     Call h5dopen_f(file_id, HEADER_DSET_NAME, header_dset_id, error)

     ! ! ! ! Read the header dataset
     Call h5dread_f(header_dset_id, H5T_NATIVE_DOUBLE, header_data, HEADER_DATA_DIMS, error)

     ! ! ! ! Unpack header_data
     x1 = header_data(1)
     x2 = header_data(2)
     this%dtOut_ = header_data(3)
     this%Tstop_ = header_data(4)
     this%nonDimxLen_ = header_data(5)
     this%nonDimyLen_ = header_data(6)
     this%nonDimdepth_ = header_data(7)
     this%gravi = header_data(8)
     this%dimL_ = header_data(9)
     this%dimT_ = header_data(10)

     ! ! ! Close the header dataset
     Call h5dclose_f(header_dset_id, error)

     ! ! Close the file
     Call h5fclose_f(file_id, error)

     ! Close FORTRAN interface
     Call h5close_f(error)

 end subroutine
#endif

!!!- read HOS Ocean mode array from HOS_Ocean results file
subroutine read_mod_HOSOcean(this, iTime)
    ! --------------------------------------------------
    !   Input
    !       iTime : time index
    !           - iTime = 0;            time = 0
    !           - iTime = nHOSTime_;    time = T_stop_
    ! --------------------------------------------------
    implicit none
    class(typHOSOcean), intent(inout) :: this
    integer,intent(in) :: iTime
    integer :: i1,i2, NRECL
    !!!......................................

    ! Time index check
    if (iTime < 0 .or. iTime > this%nHOSTime_) then
      write(*,*) "[ERROR] modGrid2GridType::read_mod(iTime)"
      write(*,*) " "
      write(*,*) "    iTime exceeds total number of simulation step"
      write(*,*) " "
      stop
    endif

    if (this%isHDF5Format_) then
#ifdef ENABLE_HDF5
        Call this%read_hdf5_mod(iTime)
#else
        write(*,*) " NO HDF library is supported. Please Compile with HDF5 Option."
        stop
#endif
    else
        Call this%read_ascii_mod(iTime)
    endif

  end subroutine

  subroutine read_ascii_mod_HOSOcean(this, iTime)
    implicit none
    class(typHOSOcean), intent(inout) :: this
    integer,intent(in) :: iTime
    integer :: i1,i2, NRECL

    ! HOS Ocean file open
    OPEN(unit = this%hosFile_%unit, &
      file = this%hosFile_%name, &
      status='OLD', &
      form='FORMATTED', &
      access='DIRECT', &
      recl=18*(2 * this%nXo2p1Org_))

    ! HOS Ocean mode file read
    if (this%hosProcedure_=='velocity') then
      NRECL = (iTime + 1)*this%nYmodeOrg_*6

      do i2=1,this%nYmodeOrg_
        READ(this%hosFile_%unit,1001,REC=NRECL+1+6*(i2-1)) (this%hosMode_%modeX(i1,i2), i1=1,this%nXo2p1Org_)
        READ(this%hosFile_%unit,1001,REC=NRECL+2+6*(i2-1)) (this%hosMode_%modeY(i1,i2), i1=1,this%nXo2p1Org_)
        READ(this%hosFile_%unit,1001,REC=NRECL+3+6*(i2-1)) (this%hosMode_%modeZ(i1,i2), i1=1,this%nXo2p1Org_)
        READ(this%hosFile_%unit,1001,REC=NRECL+4+6*(i2-1)) (this%hosMode_%modet(i1,i2), i1=1,this%nXo2p1Org_)
        READ(this%hosFile_%unit,1001,REC=NRECL+5+6*(i2-1)) (this%hosMode_%modeFS(i1,i2), i1=1,this%nXo2p1Org_)
        READ(this%hosFile_%unit,1001,REC=NRECL+6+6*(i2-1)) (this%hosMode_%modeFSt(i1,i2), i1=1,this%nXo2p1Org_)
      end do
    elseif (this%hosProcedure_=='potential') then
      NRECL = (iTime + 1)*this%nYmodeOrg_*4
      do i2=1,this%nYmode_
        READ(this%hosFile_%unit,1001,REC=NRECL+1+4*(i2-1)) (this%hosMode_%mode(i1,i2), i1=1,this%nXo2p1Org_)
        READ(this%hosFile_%unit,1001,REC=NRECL+2+4*(i2-1)) (this%hosMode_%modet(i1,i2), i1=1,this%nXo2p1Org_)
        READ(this%hosFile_%unit,1001,REC=NRECL+3+4*(i2-1)) (this%hosMode_%modeFS(i1,i2), i1=1,this%nXo2p1Org_)
        READ(this%hosFile_%unit,1001,REC=NRECL+4+4*(i2-1)) (this%hosMode_%modeFSt(i1,i2), i1=1,this%nXo2p1Org_)
      end do
    endif


    CLOSE(this%hosFile_%unit)    ! file close

    ! file reading format for HOS Ocean
1001    format((5000(ES17.10,1X)))
  end subroutine

#ifdef ENABLE_HDF5
subroutine read_hdf5_mod_HOSOcean(this, iTime)
    implicit none
    class(typHOSOcean), intent(inout) :: this
    integer,intent(in) :: iTime

    ! Time group format
    CHARACTER(len=6), parameter :: TIME_C_FORMAT = '(I0.6)' ! Increase .6) if necessary
    CHARACTER(len=6) :: time_c ! Increase len=6 if necessary

    INTEGER :: n
    INTEGER(hid_t) :: file_id, time_group_id
    INTEGER :: error
    INTEGER(hsize_t), dimension(2) :: ocean_mode_data_dims

    ! Open FORTRAN interface
    Call h5open_f(error)

    ! ! Open the file
    Call h5fopen_f(this%hosFile_%name, H5F_ACC_RDONLY_F, file_id, error)

    ! ! ! Open the iTime group
    if (iTime > 999999) then
      write(error_unit, '(2A)') "Implementation error in read_hdf5_mod: " // &
        "Unexpected iTime value > 999999 ! Modify 'time_c' variable size and 'TIME_C_FORMAT'"
      stop
    endif
    write(time_c,TIME_C_FORMAT) iTime
    Call h5gopen_f(file_id, "/time_"//trim(time_c), time_group_id, error)

    ! ! ! ! Read datasets
    ! ! ! ! nXmode_ x nYmode_
    ocean_mode_data_dims(1) = this%nXo2p1_
    ocean_mode_data_dims(2) = this%nYmode_
    Call read_hdf5_dataset_mod(time_group_id, "modeX", ocean_mode_data_dims, this%hosMode_%modeX)
    Call read_hdf5_dataset_mod(time_group_id, "modeY", ocean_mode_data_dims, this%hosMode_%modeY)
    Call read_hdf5_dataset_mod(time_group_id, "modeZ", ocean_mode_data_dims, this%hosMode_%modeZ)
    Call read_hdf5_dataset_mod(time_group_id, "modet", ocean_mode_data_dims, this%hosMode_%modet)
    Call read_hdf5_dataset_mod(time_group_id, "modeFS", ocean_mode_data_dims, this%hosMode_%modeFS)
    Call read_hdf5_dataset_mod(time_group_id, "modeFSt", ocean_mode_data_dims, this%hosMode_%modeFSt)

    ! ! ! Close iTime group
    Call h5gclose_f(time_group_id, error)

    ! ! Close the file
    Call h5fclose_f(file_id, error)

    ! Close FORTRAN interface
    Call h5close_f(error)

end subroutine
#endif

!!- Build HOS NWT Global Mesh & calculate Wave numbers and additional mode part
subroutine buildGlobalMesh_HOSOcean(this, zMin, zMax, nZmin, nZmax, zMinRatio, zMaxRatio)
    Implicit none
    class(typHOSOcean), intent(inout) :: this
    real(rp),intent(in) :: zMin,  zMax
    integer, intent(in) :: nZmin, nZmax
    real(rp), optional  :: zMinRatio, zMaxRatio
    Real(RP) :: twoPiXLen, twoPiYLen
    integer  :: nDer(2)
    integer  :: ix, iy
    integer  :: nxFou, nyFou
    real(rp) :: kXY
    !!- Build global mesh -------------------

    this%zMin_ = zMin
    this%zMax_ = zMax

    !.. Truncation Error for zMin
    if (abs(this%zMin_ + this%depth_).le.convErr) then
        this%zMin_ = -this%depth_
    endif

    !-- Check z domain length
    if (-this%zMin_.gt.this%depth_) then
        write(*,*) "    [Error] oceanSurf2Vol::buildGlobalMesh(zMin, zMax, nZmin, nZmax)"
        write(*,*) "        zMin is over than water depth !"
        write(*,*) "        zMin      : ", -this%zMin_
        write(*,*) "        HOS depth : ", this%depth_
        stop
    endif

    this%nonDimzMin_ = this%zMin_ / this%dimL_
    this%nonDimzMax_ = this%zMax_ / this%dimL_

    !-- Build Mesh
    this%hosMesh_%dimL = this%dimL_
    this%hosMesh_%dimT = this%dimT_

    Call this%hosMesh_%buildHOSOceanMesh(this%nonDimxLen_, this%nonDimyLen_, &
                                         this%nonDimzMin_, this%nonDimzMax_, &
                                         this%nXmode_, this%nYmode_, &
                                         nZmin, nZmax, zMinRatio, zMaxRatio)

    !... general mode flow quantities in Fourier Domain
    allocate( this%hosMesh_%aVitX(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%aVitY(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%aVitZ(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%aPhit(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%aDuDt(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%aDvDt(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%aDwDt(this%nXo2p1_, this%nYmode_) )

    allocate( this%hosMesh_%adVitXdx(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%adVitYdy(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%adVitXdy(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%adVitXdz(this%nXo2p1_, this%nYmode_) )
    allocate( this%hosMesh_%adVitYdz(this%nXo2p1_, this%nYmode_) )

    !-- Build Wave Number
    ! Specify temporary number of points
    nDer(1) = this%nXo2p1_
    nDer(2) = this%nYo2p1_

    twoPiXLen = TWOPI / this%nonDimxLen_

    if (this%nYmode_.eq.1) then
        twoPiYLen = 0.0_RP
    else
        twoPiYLen = TWOPI / this%nonDimyLen_
    endif

    do ix = 1, this%nXo2p1_
        this%hosMode_%kx(ix) = real(ix - 1,RP) * twoPiXLen
    enddo

    do iy = 1, this%nYo2p1_
        this%hosMode_%kyN2(iy) = real(iy - 1,RP) * twoPiYLen
    enddo

    do iy = 2, this%nYo2p1_
        this%hosMode_%kyN2(this%nYmode_ - iy + 2) = -real(iy - 1,RP) * twoPiYLen
    enddo

    if (this%isYeven_) this%hosMode_%kyN2(this%nYo2p1_) = real(this%nYo2p1_ - 1,RP) * twoPiYLen

    nxFou = MIN(nDer(1), this%nXo2p1_)
    nyFou = MIN(nDer(2), this%nYo2p1_)

    this%hosMode_%ikx = 0.0_CP

    !  x-derivative on n1 points (i.e. n1o2p1 modes)
    do iy = 1, this%nYo2p1_
        this%hosMode_%ikx(1:nxFou, iy ) = iMaginary * this%hosMode_%kx(1:nxFou)
    enddo

    ! Last mode contains cos information only and must not be part of the differentiation.
    if (this%isXeven_) this%hosMode_%ikx(this%nXo2p1_, :) = 0.0_CP

    ! negative kx
    do iy = 2, this%nYo2p1_
        this%hosMode_%ikx(1:nxFou, this%nYmode_ - iy + 2) = iMaginary * this%hosMode_%kx(1:nxFou)
    enddo

    this%hosMode_%iky = 0.0_CP

    ! y-derivative on n1 points (i.e. n1o2p1 modes)
    do ix = 1, this%nXo2p1_
        this%hosMode_%iky(ix, 1:nyFou) = iMaginary * this%hosMode_%kyN2(1:nyFou)
        ! negative ky
        do iy = 2, nyFou
            this%hosMode_%iky(ix, this%nYmode_ - iy + 2) = -iMaginary * this%hosMode_%kyN2(iy)
        enddo
        if (this%isYeven_.and.(nDer(2).ge.this%nYo2p1_)) this%hosMode_%iky(ix, this%nYo2p1_) = 0.0_CP
    enddo

    do iy = 1, this%nYmode_
        do ix = 1, this%nXo2p1_
            kXY = sqrt(this%hosMode_%kx(ix) * this%hosMode_%kx(ix) &
                      +this%hosMode_%kyN2(iy) * this%hosMode_%kyN2(iy))

            this%hosMode_%kXY(ix,iy) = kXY
            this%hosMode_%ktheta(ix,iy) = kXY * tanh(kXY * this%nonDimDepth_)
        enddo
    enddo

end subroutine

subroutine reconstructionFFTs_HOSOcean(this)
    implicit none
    class(typHOSOcean), intent(inout) :: this
    integer  :: ix, iy, iz
    real(rp) :: hosZ, eta, detadx, detady,detadt
    real(rp) :: kZ, kD, coeff, coeff2, k
    !!!......................................

    !! - Compute nondimensionalized wave elevation
    Call this%hosOceanFFTW_%fourier_2_space(this%hosMode_%modeFS,this%hosMesh_%nonDimEta)
    Call this%hosOceanFFTW_%fourier_2_space(this%hosMode_%modeFSt,this%hosMesh_%nonDimdEtadt)

    do ix = 1, this%nXo2p1_
      do iy = 1, this%nYmode_
        this%hosMode_%modeFSx(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMode_%modeFS(ix, iy) !this%hosMode_%kx(ix)
        this%hosMode_%modeFSy(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMode_%modeFS(ix, iy) !this%hosMode_%kyN2(iy)
      enddo
    end do

    Call this%hosOceanFFTW_%fourier_2_space(this%hosMode_%modeFSx,this%hosMesh_%nonDimdEtadx)
    Call this%hosOceanFFTW_%fourier_2_space(this%hosMode_%modeFSy,this%hosMesh_%nonDimdEtady)

    do iz = 1, this%hosMesh_%nZ
        hosZ = this%hosMesh_%nonDimZ(iz)

        ix = 1
        iy = 1

        if (this%hosProcedure_=='velocity') then
            this%hosMesh_%aVitX(ix, iy) = this%hosMode_%modeX(ix, iy)
            this%hosMesh_%aVitY(ix, iy) = this%hosMode_%modeY(ix, iy)
            this%hosMesh_%aVitZ(ix, iy) = this%hosMode_%modeZ(ix, iy)
        elseif (this%hosProcedure_=='potential') then
            this%hosMesh_%aVitX(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMode_%mode(ix, iy)
            this%hosMesh_%aVitY(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMode_%mode(ix, iy)
            this%hosMesh_%aVitZ(ix, iy) = this%hosMode_%ktheta(ix,iy) * this%hosMode_%mode(ix, iy)
        endif

        this%hosMesh_%aPhit(ix, iy) = this%hosMode_%modet(ix, iy)

        !if (reconstruct_space_derivative) THEN
        this%hosMesh_%adVitXdx(ix, iy) = -this%hosMode_%ikx(ix, iy) * this%hosMesh_%aVitX(ix, iy)
        this%hosMesh_%adVitYdy(ix, iy) = -this%hosMode_%iky(ix, iy) * this%hosMesh_%aVitY(ix, iy)

        this%hosMesh_%adVitXdy(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMesh_%aVitX(ix, iy)
        this%hosMesh_%adVitXdz(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMesh_%aVitZ(ix, iy)
        this%hosMesh_%adVitYdz(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMesh_%aVitZ(ix, iy)
        !endif

        ! this%hosMesh_%aDuDt(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMesh_%aPhit(ix, iy)
        ! this%hosMesh_%aDvDt(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMesh_%aPhit(ix, iy)
        ! this%hosMesh_%aDwDt(ix, iy) = this%hosMode_%ktheta(ix, iy) * this%hosMode_%modet(ix, iy)

        do iy = 2, this%nYmode_
            k  = this%hosMode_%kXY(ix, iy)
            kZ = k * (hosZ + this%nonDimDepth_ )
            kD = k * this%nonDimDepth_
            if ( (kZ.lt.50.0).and.(kD.le.50.0) ) then
                ! coeff  = cosh(kZ) / cosh(kD)
                ! coeff2 = sinh(kZ) / sinh(kD)
                coeff  = exp(k * hosZ) * (1.0_rp + exp(-2.0_rp*kZ)) / (1.0_rp + exp(-2.0_rp * kD))
                coeff2 = exp(k * hosZ) * (1.0_rp - exp(-2.0_rp*kZ)) / (1.0_rp - exp(-2.0_rp * kD))
            else
                coeff  = exp(this%hosMode_%kXY(ix, iy) * hosZ)
                coeff2 = coeff
            endif

            if (coeff.ge.FNZ_VALUE) then
                coeff = FNZ_VALUE
            endif

            if (coeff2.ge.FNZ_VALUE) then
                coeff2 = FNZ_VALUE
            endif

            if (this%hosProcedure_=='velocity') then
                 this%hosMesh_%aVitX(ix, iy) = this%hosMode_%modeX(ix, iy) * coeff
                 this%hosMesh_%aVitY(ix, iy) = this%hosMode_%modeY(ix, iy) * coeff
                 this%hosMesh_%aVitZ(ix, iy) = this%hosMode_%modeZ(ix, iy) * coeff2
            elseif (this%hosProcedure_=='potential') then
                this%hosMesh_%aVitX(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMode_%mode(ix, iy) * coeff
                this%hosMesh_%aVitY(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMode_%mode(ix, iy) * coeff
                this%hosMesh_%aVitZ(ix, iy) = this%hosMode_%ktheta(ix,iy) * this%hosMode_%mode(ix, iy) * coeff2
            endif
            this%hosMesh_%aPhit(ix, iy) = this%hosMode_%modet(ix, iy) * coeff

            !if (reconstruct_space_derivative) THEN
            this%hosMesh_%adVitXdx(ix, iy) = -this%hosMode_%ikx(ix, iy) * this%hosMesh_%aVitX(ix, iy)
            this%hosMesh_%adVitYdy(ix, iy) = -this%hosMode_%iky(ix, iy) * this%hosMesh_%aVitY(ix, iy)
            this%hosMesh_%adVitXdy(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMesh_%aVitX(ix, iy)
            this%hosMesh_%adVitXdz(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMesh_%aVitZ(ix, iy)
            this%hosMesh_%adVitYdz(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMesh_%aVitZ(ix, iy)
            !endif

            ! this%hosMesh_%aDuDt(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMesh_%aPhit(ix, iy)
            ! this%hosMesh_%aDvDt(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMesh_%aPhit(ix, iy)
            ! this%hosMesh_%aDwDt(ix, iy) = this%hosMode_%ktheta(ix, iy) * this%hosMode_%modet(ix, iy) * coeff2
        enddo

        do ix = 2, this%nXo2p1_
            do iy = 1, this%nYmode_
                kZ = this%hosMode_%kXY(ix, iy) * (hosZ + this%nonDimDepth_ )
                kD = this%hosMode_%kXY(ix, iy) * this%nonDimDepth_

                if ( (kZ.lt.50.0).and.(kD.le.50.0) ) then
                    coeff  = cosh(kZ) / cosh(kD)
                    coeff2 = sinh(kZ) / sinh(kD)
                else
                    coeff  = exp(this%hosMode_%kXY(ix, iy) * hosZ)
                    coeff2 = coeff
                endif

                if (coeff.ge.1000.0_RP) then
                    coeff = 1000.0_RP
                endif

                if (coeff2.ge.1000.0_RP) then
                    coeff2 = 1000.0_RP
                endif

                if (this%hosProcedure_=='velocity') then
                     this%hosMesh_%aVitX(ix, iy) = this%hosMode_%modeX(ix, iy) * coeff
                     this%hosMesh_%aVitY(ix, iy) = this%hosMode_%modeY(ix, iy) * coeff
                     this%hosMesh_%aVitZ(ix, iy) = this%hosMode_%modeZ(ix, iy) * coeff2
                elseif (this%hosProcedure_=='potential') then
                    this%hosMesh_%aVitX(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMode_%mode(ix, iy) * coeff
                    this%hosMesh_%aVitY(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMode_%mode(ix, iy) * coeff
                    this%hosMesh_%aVitZ(ix, iy) = this%hosMode_%ktheta(ix,iy) * this%hosMode_%mode(ix, iy) * coeff2
                endif

                this%hosMesh_%aPhit(ix, iy) = this%hosMode_%modet(ix, iy) * coeff

                !if (reconstruct_space_derivative) THEN
                this%hosMesh_%adVitXdx(ix, iy) = -this%hosMode_%ikx(ix, iy) * this%hosMesh_%aVitX(ix, iy)
                this%hosMesh_%adVitYdy(ix, iy) = -this%hosMode_%iky(ix, iy) * this%hosMesh_%aVitY(ix, iy)
                this%hosMesh_%adVitXdy(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMesh_%aVitX(ix, iy)
                this%hosMesh_%adVitXdz(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMesh_%aVitZ(ix, iy)
                this%hosMesh_%adVitYdz(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMesh_%aVitZ(ix, iy)
                !endif

                ! this%hosMesh_%aDuDt(ix, iy) = this%hosMode_%ikx(ix, iy) * this%hosMesh_%aPhit(ix, iy)
                ! this%hosMesh_%aDvDt(ix, iy) = this%hosMode_%iky(ix, iy) * this%hosMesh_%aPhit(ix, iy)
                ! this%hosMesh_%aDwDt(ix, iy) = this%hosMode_%ktheta(ix, iy) * this%hosMode_%modet(ix, iy) * coeff2
            enddo
        enddo

        Call this%hosOceanFFTW_%fourier_2_space(this%hosMesh_%aVitX, this%hosMesh_%nonDimPhiX(:,:, iz) )

        if (this%nYmode_.NE.1) then
            Call this%hosOceanFFTW_%fourier_2_space(this%hosMesh_%aVitY, this%hosMesh_%nonDimPhiY(:,:, iz) )
            ! Call this%hosOceanFFTW_%fourier_2_space( this%hosMesh_%aDvDt, this%hosMesh_%nonDimDvDt(:,:, iz) )
        else
            this%hosMesh_%nonDimPhiY(:,:, iz) = 0.0_rp
            ! this%hosMesh_%nonDimDvDt(:,:, iz) = 0.0_rp
        end if

        Call this%hosOceanFFTW_%fourier_2_space(this%hosMesh_%aVitZ, this%hosMesh_%nonDimPhiZ(:,:, iz) )
        Call this%hosOceanFFTW_%fourier_2_space(this%hosMesh_%aPhit, this%hosMesh_%nonDimPhit(:,:, iz) )

        Call this%hosOceanFFTW_%fourier_2_space(this%hosMesh_%adVitXdx, this%hosMesh_%nonDimdvitXdX(:,:, iz) )
        Call this%hosOceanFFTW_%fourier_2_space(this%hosMesh_%adVitYdy, this%hosMesh_%nonDimdvitYdY(:,:, iz) )
        Call this%hosOceanFFTW_%fourier_2_space(this%hosMesh_%adVitXdy, this%hosMesh_%nonDimdvitXdY(:,:, iz) )
        Call this%hosOceanFFTW_%fourier_2_space(this%hosMesh_%adVitXdz, this%hosMesh_%nonDimdvitXdZ(:,:, iz) )
        Call this%hosOceanFFTW_%fourier_2_space(this%hosMesh_%adVitYdz, this%hosMesh_%nonDimdvitYdZ(:,:, iz) )

        ! Call this%hosOceanFFTW_%fourier_2_space( this%hosMesh_%aDuDt, this%hosMesh_%nonDimDuDt(:,:, iz) )
        ! Call this%hosOceanFFTW_%fourier_2_space( this%hosMesh_%aDwDt, this%hosMesh_%nonDimDwDt(:,:, iz) )

    enddo

end subroutine
