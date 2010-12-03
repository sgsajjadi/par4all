/** @file

    API of Par4All C to OpenMP

    This file is an implementation of the C to accelerator Par4All API.

    Funded by the FREIA (French ANR), TransMedi\@ (French P�le de
    Comp�titivit� Images and Network) and SCALOPES (Artemis European
    Project project)

    "mailto:Stephanie.Even@enstb.org"
    "mailto:Ronan.Keryell@hpc-project.com"
*/

#include <p4a_accel.h>


#ifdef P4A_ACCEL_OPENMP

/** Stop a timer on the accelerator and get double ms time

    @addtogroup P4A_OpenMP_time_measure
*/

double P4A_accel_timer_stop_and_float_measure() {
  double run_time;
  gettimeofday(&p4a_time_end, NULL);
  /* Take care of the non-associativity in floating point :-) */
  run_time = (p4a_time_end.tv_sec - p4a_time_begin.tv_sec)
    + (p4a_time_end.tv_usec - p4a_time_begin.tv_usec)*1e-6;
  return run_time;
}


/** This is a global variable used to simulate P4A virtual processor
    coordinates in OpenMP because we need to pass a local variable to a
    function without passing it in the arguments.

    Use thead local storage to have it local to each OpenMP thread.

    With __thread, it looks like this declaration cannot be repeated in
    the .h without any extern.
 */
__thread int P4A_vp_coordinate[P4A_vp_dim_max];


/** @defgroup P4A_OpenMP_memory_allocation_copy Memory allocation and copy for OpenMP simulation

    @{
*/

/** Allocate memory on the hardware accelerator in OpenMP emulation.

    For OpenMP it is on the host too.

    @param[out] address is the address of a variable that is updated by
    this macro to contains the address of the allocated memory block

    @param[in] size is the size to allocate in bytes
*/
void P4A_accel_malloc(void **address, size_t size) {
  *address = malloc(size);
}


/** Free memory on the hardware accelerator in OpenMP emulation.

    It is on the host too

    @param[in] address points to a previously allocated memory zone for
    the hardware accelerator
*/
void P4A_accel_free(void *address) {
  free(address);
}

/** Copy a scalar from the hardware accelerator to the host in OpenMP
 emulation.

 Since it is an OpenMP implementation, use standard memory copy
 operations

 Do not change the place of the pointers in the API. The host address
 is always first...

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_from_accel(size_t element_size,
			 void *host_address,
			 const void *accel_address) {
  /* We can use memcpy() since we are sure there is no overlap */
  memcpy(host_address, accel_address, element_size);
}

/** Copy a scalar from the host to the hardware accelerator in OpenMP
 emulation.

 Since it is an OpenMP implementation, use standard memory copy
 operations

 Do not change the place of the pointers in the API. The host address
 is always before the accel address...

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_to_accel(size_t element_size,
		       const void *host_address,
		       void *accel_address) {
  /* We can use memcpy() since we are sure there is no overlap */
  memcpy(accel_address, host_address, element_size);
}

/** Function for copying memory from the hardware accelerator to a 1D array in
 the host.

 This function could be quite simpler but is designed by symmetry with
 other functions.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from  (host side)

 @param[out] host_address point to the array on the host to write into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_from_accel_1d(size_t element_size,
			    size_t d1_size,
			    size_t d1_block_size,
			    size_t d1_offset,
			    void *host_address,
			    const void *accel_address) {

  // Compute the destination address on host side
  char * cdest = d1_offset*element_size + (char *)host_address;
  const char * csrc = (char*)accel_address;
  // Copy element by element
  for(size_t i = 0; i < d1_block_size*element_size; i++)
    cdest[i] = csrc[i];
}

/** Function for copying a 1D memory zone from the host to a compact memory
 zone in the hardware accelerator.

 This function could be quite simpler but is designed by symmetry with
 other functions.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from

 @param[in] host_address point to the array on the host to read

 @param[out] accel_address refer to the compact memory area to write
 data. In the general case, accel_address may be seen as a unique idea
 (FIFO) and not some address in some memory space.
 */
void P4A_copy_to_accel_1d(size_t element_size,
    size_t d1_size,
    size_t d1_block_size,
    size_t d1_offset,
    const void *host_address,
    void *accel_address) {
  // Compute the destination address on host side
  const char * csrc = d1_offset*element_size + (char *)host_address;
  char * cdest = (char*)accel_address;

  // Copy element by element
  for(size_t i = 0; i < d1_block_size*element_size; i++)
    cdest[i] = csrc[i];
}

/** Function for copying memory from the hardware accelerator to a 2D array in
 the host.
 */
void P4A_copy_from_accel_2d(size_t element_size,
			    size_t d1_size, size_t d2_size,
			    size_t d1_block_size, size_t d2_block_size,
			    size_t d1_offset, size_t d2_offset,
			    void *host_address,
			    const void *accel_address) {
  // Compute the destination address for the first rown on dim1 on host side
  char * cdest = d2_offset*element_size + (char*)host_address;
  const char * csrc = (char*)accel_address;

  // Copy element by element
  for(size_t i = 0; i < d1_block_size; i++)
    for(size_t j = 0; j < d2_block_size*element_size; j++)
      cdest[(i + d1_offset)*element_size*d2_size + j] =
          csrc[i*element_size*d2_block_size + j];
}

/** Function for copying a 2D memory zone from the host to a compact memory
 zone in the hardware accelerator.
 */
void P4A_copy_to_accel_2d(size_t element_size,
    size_t d1_size, size_t d2_size,
    size_t d1_block_size, size_t d2_block_size,
    size_t d1_offset, size_t d2_offset,
    const void *host_address,
    void *accel_address) {
  char * cdest = (char *)accel_address;
  const char * csrc = d2_offset*element_size + (char *)host_address;
  for(size_t i = 0; i < d1_block_size; i++)
    for(size_t j = 0; j < d2_block_size*element_size; j++)
      cdest[i*element_size*d2_block_size + j] = csrc[(i + d1_offset)*element_size*d2_size + j];
}

/** Function for copying memory from the hardware accelerator to a 3D array in
    the host.
*/
void P4A_copy_from_accel_3d(size_t element_size,
			    size_t d1_size, size_t d2_size, size_t d3_size,
			    size_t d1_block_size, 
			    size_t d2_block_size, size_t d3_block_size,
			    size_t d1_offset, 
			    size_t d2_offset, size_t d3_offset,
			    void *host_address,
			    const void *accel_address) {
  char * cdest = d3_offset*element_size + (char*)host_address;
  const char * csrc = (char*)accel_address;
  for(size_t i = 0; i < d1_block_size; i++)
    for(size_t j = 0; j < d2_block_size; j++)
      for(size_t k = 0; k < d3_block_size*element_size; k++)
	cdest[((i + d1_offset)*d2_block_size + j + d2_offset)*element_size*d3_size + k] =
	      csrc[(i*d2_block_size + j)*d3_block_size*element_size + k];
}


/** Function for copying a 3D memory zone from the host to a compact memory
    zone in the hardware accelerator.
*/
void P4A_copy_to_accel_3d(size_t element_size,
			  size_t d1_size, size_t d2_size, size_t d3_size,
			  size_t d1_block_size, size_t d2_block_size, size_t d3_block_size,
			  size_t d1_offset,   size_t d2_offset, size_t d3_offset,
			  const void *host_address,
			  void *accel_address) {
  char * cdest = (char *)accel_address;
  const char * csrc = d3_offset*element_size + (char *)host_address;
  for(size_t i = 0; i < d1_block_size; i++)
    for(size_t j = 0; j < d2_block_size; j++)
      for(size_t k = 0; k < d3_block_size*element_size; k++)
	cdest[(i*d2_block_size + j)*d3_block_size*element_size + k] =
	  csrc[((i + d1_offset)*d2_block_size + j + d2_offset)*element_size*d3_size + k];
}

/* @} */
#endif // P4A_ACCEL_OPENMP

#ifdef P4A_ACCEL_CUDA

//#include <cutil_inline.h>

/** To do basic time measure. Do not nest... */

cudaEvent_t p4a_start_event, p4a_stop_event;

/** Stop a timer on the accelerator and get float time in second

 @addtogroup P4A_cuda_time_measure
 */

double P4A_accel_timer_stop_and_float_measure() {
  float execution_time;
  P4A_test_execution(cudaEventRecord(p4a_stop_event, 0));
  P4A_test_execution(cudaEventSynchronize(p4a_stop_event));
  /* Get the time in ms: */
  P4A_test_execution(cudaEventElapsedTime(&execution_time,
          p4a_start_event,
          p4a_stop_event));
  /* Return the time in second: */
  return execution_time*1e-3;
}



/** Allocate memory on the hardware accelerator in Cuda mode.

    @param[out] address is the address of a variable that is updated by
    this function to contains the address of the allocated memory block

    @param[in] size is the size to allocate in bytes
*/
void P4A_accel_malloc(void **address, size_t size) {
  P4A_test_execution(cudaMalloc(address, size));
}


/** Free memory on the hardware accelerator in Cuda mode

    @param[in] address points to a previously allocated memory zone for
    the hardware accelerator
*/
void P4A_accel_free(void *address) {
  P4A_test_execution(cudaFree(address));
}

/** Copy a scalar from the hardware accelerator to the host

 It's a wrapper around CudaMemCopy*.

 Do not change the place of the pointers in the API. The host address
 is always first...

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_from_accel(size_t element_size,
			 void *host_address,
			 const void *accel_address) {
  cudaMemcpy(host_address,accel_address,element_size,cudaMemcpyDeviceToHost);
}

/** Copy a scalar from the host to the hardware accelerator

 It's a wrapper around CudaMemCopy*.

 Do not change the place of the pointers in the API. The host address
 is always before the accel address...

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_to_accel(size_t element_size,
		       const void *host_address,
		       void *accel_address) {
  cudaMemcpy(accel_address,host_address,element_size,cudaMemcpyHostToDevice);
}

/** Function for copying memory from the hardware accelerator to a 1D array in
 the host.

 It's a wrapper around CudaMemCopy*.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from (host side)

 @param[out] host_address point to the array on the host to write into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.

 @return the host_address, by compatibility with memcpy().
 */
void P4A_copy_from_accel_1d(size_t element_size,
			    size_t d1_size,
			    size_t d1_block_size,
			    size_t d1_offset,
			    void *host_address,
			    const void *accel_address) {
  char * cdest = d1_offset*element_size + (char *)host_address;
  cudaMemcpy(cdest,accel_address,d1_block_size*element_size,cudaMemcpyDeviceToHost);
}

/** Function for copying a 1D memory zone from the host to a compact memory
 zone in the hardware accelerator.

 This function could be quite simpler but is designed by symmetry with
 other functions.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from

 @param[in] host_address point to the array on the host to read

 @param[out] accel_address refer to the compact memory area to write
 data. In the general case, accel_address may be seen as a unique idea
 (FIFO) and not some address in some memory space.
 */
void P4A_copy_to_accel_1d(size_t element_size,
    size_t d1_size,
    size_t d1_block_size,
    size_t d1_offset,
    const void *host_address,
    void *accel_address) {
  const char * csrc = d1_offset*element_size + (char *)host_address;
  cudaMemcpy(accel_address,csrc,d1_block_size*element_size,cudaMemcpyHostToDevice);
}

/** Function for copying memory from the hardware accelerator to a 2D array in
 the host.
 */
void P4A_copy_from_accel_2d(size_t element_size,
			    size_t d1_size, size_t d2_size,
			    size_t d1_block_size, size_t d2_block_size,
			    size_t d1_offset, size_t d2_offset,
			    void *host_address,
			    const void *accel_address) {

  if(d2_size==d2_block_size ) { // d2 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_from_accel_1d(element_size,
                           d2_size * d1_size,
                           d2_size * d1_block_size,
                           d1_offset*d2_size+d2_offset,
                           host_address,
                           accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_1d(element_size,
                             d2_size,
                             d2_block_size,
                             d2_offset,
                             host_row,
                             accel_row);

      // Comput addresses for next row
      host_row += d2_size*element_size;
      accel_row += d2_block_size*element_size;
    }
  }
}

/** Function for copying a 2D memory zone from the host to a compact memory
 zone in the hardware accelerator.
 */
void P4A_copy_to_accel_2d(size_t element_size,
    size_t d1_size, size_t d2_size,
    size_t d1_block_size, size_t d2_block_size,
    size_t d1_offset, size_t d2_offset,
    const void *host_address,
    void *accel_address) {
  if(d2_size==d2_block_size ) { // d2 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_to_accel_1d(element_size,
                         d2_size * d1_size,
                         d2_size * d1_block_size,
                         d1_offset*d2_size+d2_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_1d(element_size,
                           d2_size,
                           d2_block_size,
                           d2_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d2_size*element_size;
      accel_row += d2_block_size*element_size;
    }
  }

}

/** Function for copying memory from the hardware accelerator to a 3D array in
    the host.
*/
void P4A_copy_from_accel_3d(size_t element_size,
			    size_t d1_size, size_t d2_size, size_t d3_size,
			    size_t d1_block_size, 
			    size_t d2_block_size, size_t d3_block_size,
			    size_t d1_offset, 
			    size_t d2_offset, size_t d3_offset,
			    void *host_address,
			    const void *accel_address) {


  if(d3_size==d3_block_size ) { // d3 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_from_accel_2d(element_size,
                           d1_size,d3_size * d2_size,
                           d1_block_size, d3_size * d2_block_size,
                           d1_offset, d2_offset*d3_size+d3_offset,
                           host_address,
                           accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_2d(element_size,
                             d2_size,d3_size,
                             d2_block_size, d3_block_size,
                             d2_offset, d3_offset,
                             host_row,
                             accel_row);

      // Comput addresses for next row
      host_row += d3_size*element_size;
      accel_row += d3_block_size*element_size;
    }
  }
}


/** Function for copying a 3D memory zone from the host to a compact memory
    zone in the hardware accelerator.
*/
void P4A_copy_to_accel_3d(size_t element_size,
        size_t d1_size, size_t d2_size, size_t d3_size,
        size_t d1_block_size, size_t d2_block_size, size_t d3_block_size,
        size_t d1_offset,   size_t d2_offset, size_t d3_offset,
        const void *host_address,
        void *accel_address) {

  if(d3_size==d3_block_size ) { // d3 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_to_accel_2d(element_size,
                         d1_size,d3_size * d2_size,
                         d1_block_size, d3_size * d2_block_size,
                         d1_offset, d2_offset*d3_size+d3_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_2d(element_size,
                           d2_size,d3_size,
                           d2_block_size, d3_block_size,
                           d2_offset, d3_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d3_size*element_size;
      accel_row += d3_block_size*element_size;
    }
  }
}

/** Function for copying memory from the hardware accelerator to a 4D array in
    the host.
*/
void P4A_copy_from_accel_4d(size_t element_size,
			    size_t d1_size, size_t d2_size, 
			    size_t d3_size, size_t d4_size,
			    size_t d1_block_size, size_t d2_block_size, 
			    size_t d3_block_size, size_t d4_block_size,
			    size_t d1_offset, size_t d2_offset, 
			    size_t d3_offset, size_t d4_offset,
			    void *host_address,
			    const void *accel_address) {

  if(d4_size==d4_block_size ) { // d4 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_from_accel_3d(element_size,
                         d1_size,d2_size,d3_size * d4_size,
                         d1_block_size, d2_block_size, d3_block_size * d4_size,
                         d1_offset, d2_offset, d3_offset*d4_size+d4_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*d4_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_3d(element_size,
                           d2_size,d3_size,d4_size,
                           d2_block_size, d3_block_size, d4_block_size,
                           d2_offset, d3_offset, d4_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d4_size*element_size;
      accel_row += d4_block_size*element_size;
    }
  }
}

/** Function for copying a 4D memory zone from the host to a compact memory
    zone in the hardware accelerator.
*/
void P4A_copy_to_accel_4d(size_t element_size,
			  size_t d1_size, size_t d2_size, 
			  size_t d3_size, size_t d4_size,
			  size_t d1_block_size, size_t d2_block_size, 
			  size_t d3_block_size, size_t d4_block_size,
			  size_t d1_offset, size_t d2_offset, 
			  size_t d3_offset, size_t d4_offset,
			  const void *host_address,
			  void *accel_address) {
  if(d4_size==d4_block_size ) { // d4 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_to_accel_3d(element_size,
                         d1_size,d2_size,d3_size * d4_size,
                         d1_block_size, d2_block_size, d3_block_size * d4_size,
                         d1_offset, d2_offset, d3_offset*d4_size+d4_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*d4_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_3d(element_size,
                           d2_size,d3_size,d4_size,
                           d2_block_size, d3_block_size, d4_block_size,
                           d2_offset, d3_offset, d4_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d4_size*element_size;
      accel_row += d4_block_size*element_size;
    }
  }
}


#endif //P4A_ACCEL_CUDA

#ifdef P4A_ACCEL_CL

#ifdef P4A_PROFILING
cl_command_queue_properties p4a_queue_properties = CL_QUEUE_PROFILING_ENABLE;
#else
cl_command_queue_properties p4a_queue_properties = 0;
#endif

bool p4a_time_tag=false;
double p4a_time_execution = 0.;
double p4a_time_copy = 0.;
cl_event p4a_event_execution=NULL, p4a_event_copy=NULL;
cl_int p4a_global_error=0;
cl_context p4a_context = NULL;
cl_command_queue p4a_queue = NULL;
//cl_device_id p4a_device_id = NULL;  
//cl_platform_id p4a_platform_id = NULL;
cl_program p4a_program = NULL;  
cl_kernel p4a_kernel = NULL;  

/** Stop a timer on the accelerator and get float time in second

 @addtogroup P4A_opencl_time_measure
 */

double P4A_accel_copy_timer()
{
  cl_ulong start,end;
  double time;

#ifdef P4A_PROFILING
  if (p4a_event_copy) {
    clWaitForEvents(1, &p4a_event_copy);
    
    P4A_test_execution(clGetEventProfilingInfo(p4a_event_copy, 
					       CL_PROFILING_COMMAND_END, 
					       sizeof(cl_ulong), 
					       &end, 
					       NULL));
    P4A_test_execution(clGetEventProfilingInfo(p4a_event_copy, 
					       CL_PROFILING_COMMAND_START, 
					       sizeof(cl_ulong), 
					       &start, 
					       NULL));
    // execution_time in nanoseconds	      
    time = (float)(end - start);
    //printf("p4a_time_copy : %f\n",time*1.0e-9);
    // Return the time in second: 
    return time*1.0e-9;
  }
#endif
  return 0;
}

double P4A_accel_timer_stop_and_float_measure() 
{
  cl_ulong start,end;

#ifdef P4A_PROFILING
  if (p4a_event_execution) {
    clWaitForEvents(1, &p4a_event_execution);
    
    P4A_test_execution(clGetEventProfilingInfo(p4a_event_execution, 
					       CL_PROFILING_COMMAND_END, 
					       sizeof(cl_ulong), 
					       &end, 
					       NULL));
    P4A_test_execution(clGetEventProfilingInfo(p4a_event_execution, 
					       CL_PROFILING_COMMAND_START, 
					       sizeof(cl_ulong), 
					       &start, 
					       NULL));
    // execution_time in nanoseconds	      
    p4a_time_execution += (float)(end - start);
    // Return the time in second:
    return p4a_time_execution*1.0e-9;
  }
#endif
  return 0;
}

/** Allocate memory on the hardware accelerator in OpenCL mode.

    @param[out] address is the address of a variable that is updated by
    this function to contains the address of the allocated memory block

    @param[in] size is the size to allocate in bytes
*/
void P4A_accel_malloc(void **address, size_t size) {
  // The mem flag can be : CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY, 
  // CL_MEM_READ_WRITE or CL_MEM_USE_HOST_PTR
  *address=(void *)clCreateBuffer(p4a_context,
				  CL_MEM_READ_WRITE,
				  size,
				  NULL,
				  &p4a_global_error);
  P4A_test_execution_with_message("Memory allocation via clCreateBuffer");
}

/** Free memory on the hardware accelerator in OpenCL mode

    @param[in] address points to a previously allocated memory zone for
    the hardware accelerator
*/
void P4A_accel_free(void *address) 
{
  p4a_global_error=clReleaseMemObject((cl_mem)address);
  P4A_test_execution_with_message("clReleaseMemObject");
}

/** Copy a scalar from the hardware accelerator to the host

 @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_from_accel(size_t element_size,
			 void *host_address,
			 const void *accel_address) 
{
  p4a_global_error=clEnqueueReadBuffer(p4a_queue,
				       (cl_mem)accel_address,
				       CL_TRUE, // synchronous read
				       0,
				       element_size,
				       host_address,
				       0,
				       NULL,
				       &p4a_event_copy);
  p4a_time_copy += P4A_accel_copy_timer();
  P4A_test_execution_with_message("clEnqueueReadBuffer");
}

/** Copy a scalar from the host to the hardware accelerator

  @param[in] element_size is the size of one element of the array in
 byte

 @param[out] host_address point to the element on the host to write
 into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.
 */
void P4A_copy_to_accel(size_t element_size,
		       const void *host_address,
		       void *accel_address) 
{
  p4a_global_error=clEnqueueWriteBuffer(p4a_queue,
					(cl_mem)accel_address,
					CL_FALSE,// asynchronous write
					0,
					element_size,
					host_address,
					0,
					NULL,
					&p4a_event_copy);
  p4a_time_copy += P4A_accel_copy_timer();
  P4A_test_execution_with_message("clEnqueueWriteBuffer");
}

/** Function for copying memory from the hardware accelerator to a 1D array in
 the host.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from (host side)

 @param[out] host_address point to the array on the host to write into

 @param[in] accel_address refer to the compact memory area to read
 data. In the general case, accel_address may be seen as a unique idea (FIFO)
 and not some address in some memory space.

 @return the host_address, by compatibility with memcpy().
 */
void P4A_copy_from_accel_1d(size_t element_size,
			    size_t d1_size,
			    size_t d1_block_size,
			    size_t d1_offset,
			    void *host_address,
			    const void *accel_address) 
{
  char * cdest = d1_offset*element_size + (char *)host_address;
  // CL_TRUE : synchronous read
  p4a_global_error=clEnqueueReadBuffer(p4a_queue,
				       (cl_mem)accel_address,
				       CL_TRUE,
				       0,
				       d1_block_size*element_size,
				       cdest,
				       0,
				       NULL,
				       &p4a_event_copy);
  p4a_time_copy += P4A_accel_copy_timer();
  P4A_test_execution_with_message("clEnqueueReadBuffer");
}


/** Function for copying a 1D memory zone from the host to a compact memory
 zone in the hardware accelerator.

 This function could be quite simpler but is designed by symmetry with
 other functions.

 @param[in] element_size is the size of one element of the array in
 byte

 @param[in] d1_size is the number of elements in the array. It is not
 used but here for symmetry with functions of higher dimensionality

 @param[in] d1_block_size is the number of element to transfer

 @param[in] d1_offset is element order to start the transfer from

 @param[in] host_address point to the array on the host to read

 @param[out] accel_address refer to the compact memory area to write
 data. In the general case, accel_address may be seen as a unique idea
 (FIFO) and not some address in some memory space.
 */
void P4A_copy_to_accel_1d(size_t element_size,
			  size_t d1_size,
			  size_t d1_block_size,
			  size_t d1_offset,
			  const void *host_address,
			  void *accel_address) 
{
  const char * csrc = d1_offset*element_size + (char *)host_address;
  p4a_global_error=clEnqueueWriteBuffer(p4a_queue,
					(cl_mem)accel_address,
					CL_FALSE, //CL_FALSE:asynchronous write
					0,
					d1_block_size*element_size,
					csrc,
					0,
					NULL,
					&p4a_event_copy);
  p4a_time_copy += P4A_accel_copy_timer();
  P4A_test_execution_with_message("clEnqueueWriteBuffer");
}

/** Function for copying memory from the hardware accelerator to a 2D array in
 the host.
 */
void P4A_copy_from_accel_2d(size_t element_size,
			    size_t d1_size, size_t d2_size,
			    size_t d1_block_size, size_t d2_block_size,
			    size_t d1_offset, size_t d2_offset,
			    void *host_address,
			    const void *accel_address) {

  if(d2_size==d2_block_size ) { // d2 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_from_accel_1d(element_size,
                           d2_size * d1_size,
                           d2_size * d1_block_size,
                           d1_offset*d2_size+d2_offset,
                           host_address,
                           accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_1d(element_size,
                             d2_size,
                             d2_block_size,
                             d2_offset,
                             host_row,
                             accel_row);

      // Comput addresses for next row
      host_row += d2_size*element_size;
      accel_row += d2_block_size*element_size;
    }
  }
}

/** Function for copying a 2D memory zone from the host to a compact memory
 zone in the hardware accelerator.
 */
void P4A_copy_to_accel_2d(size_t element_size,
			  size_t d1_size, size_t d2_size,
			  size_t d1_block_size, size_t d2_block_size,
			  size_t d1_offset, size_t d2_offset,
			  const void *host_address,
			  void *accel_address) 
{
  if(d2_size==d2_block_size ) { // d2 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_to_accel_1d(element_size,
                         d2_size * d1_size,
                         d2_size * d1_block_size,
                         d1_offset*d2_size+d2_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*element_size;
    char * accel_row = (char*)accel_address;
    
    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_1d(element_size,
                           d2_size,
                           d2_block_size,
                           d2_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d2_size*element_size;
      accel_row += d2_block_size*element_size;
    }
  }

}

/** Function for copying memory from the hardware accelerator to a 3D array in
    the host.
*/
void P4A_copy_from_accel_3d(size_t element_size,
			    size_t d1_size, size_t d2_size, size_t d3_size,
			    size_t d1_block_size, 
			    size_t d2_block_size, size_t d3_block_size,
			    size_t d1_offset, 
			    size_t d2_offset, size_t d3_offset,
			    void *host_address,
			    const void *accel_address) {


  if(d3_size==d3_block_size ) { // d3 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_from_accel_2d(element_size,
                           d1_size,d3_size * d2_size,
                           d1_block_size, d3_size * d2_block_size,
                           d1_offset, d2_offset*d3_size+d3_offset,
                           host_address,
                           accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_2d(element_size,
                             d2_size,d3_size,
                             d2_block_size, d3_block_size,
                             d2_offset, d3_offset,
                             host_row,
                             accel_row);

      // Comput addresses for next row
      host_row += d3_size*element_size;
      accel_row += d3_block_size*element_size;
    }
  }
}


/** Function for copying a 3D memory zone from the host to a compact memory
    zone in the hardware accelerator.
*/
void P4A_copy_to_accel_3d(size_t element_size,
			  size_t d1_size, size_t d2_size, size_t d3_size,
			  size_t d1_block_size, 
			  size_t d2_block_size, 
			  size_t d3_block_size,
			  size_t d1_offset, size_t d2_offset, size_t d3_offset,
			  const void *host_address,
			  void *accel_address) 
{
  if(d3_size==d3_block_size ) { // d3 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_to_accel_2d(element_size,
                         d1_size,d3_size * d2_size,
                         d1_block_size, d3_size * d2_block_size,
                         d1_offset, d2_offset*d3_size+d3_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_2d(element_size,
                           d2_size,d3_size,
                           d2_block_size, d3_block_size,
                           d2_offset, d3_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d3_size*element_size;
      accel_row += d3_block_size*element_size;
    }
  }
}

/** Function for copying memory from the hardware accelerator to a 4D array in
    the host.
*/
void P4A_copy_from_accel_4d(size_t element_size,
			    size_t d1_size, size_t d2_size, 
			    size_t d3_size, size_t d4_size,
			    size_t d1_block_size, size_t d2_block_size, 
			    size_t d3_block_size, size_t d4_block_size,
			    size_t d1_offset, size_t d2_offset, 
			    size_t d3_offset, size_t d4_offset,
			    void *host_address,
			    const void *accel_address) {

  if(d4_size==d4_block_size ) { // d4 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_from_accel_3d(element_size,
                         d1_size,d2_size,d3_size * d4_size,
                         d1_block_size, d2_block_size, d3_block_size * d4_size,
                         d1_offset, d2_offset, d3_offset*d4_size+d4_offset,
                         host_address,
                         accel_address);
  } else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*d4_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_from_accel_3d(element_size,
                           d2_size,d3_size,d4_size,
                           d2_block_size, d3_block_size, d4_block_size,
                           d2_offset, d3_offset, d4_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d4_size*element_size;
      accel_row += d4_block_size*element_size;
    }
  }
}

/** Function for copying a 4D memory zone from the host to a compact memory
    zone in the hardware accelerator.
*/
void P4A_copy_to_accel_4d(size_t element_size,
			  size_t d1_size, size_t d2_size, 
			  size_t d3_size, size_t d4_size,
			  size_t d1_block_size, size_t d2_block_size, 
			  size_t d3_block_size, size_t d4_block_size,
			  size_t d1_offset, size_t d2_offset, 
			  size_t d3_offset, size_t d4_offset,
			  const void *host_address,
			  void *accel_address) {
  if (d4_size == d4_block_size) { //d4 is fully transfered ? We can optimize :-)
    // We transfer all in one shot !
    P4A_copy_to_accel_3d(element_size,
                         d1_size,d2_size,d3_size * d4_size,
                         d1_block_size, d2_block_size, d3_block_size * d4_size,
                         d1_offset, d2_offset, d3_offset*d4_size+d4_offset,
                         host_address,
                         accel_address);
  } 
  else { // Transfer row by row !
    // Compute the adress of the begining of the first row to transfer
    char * host_row = (char*)host_address + d1_offset*d2_size*d3_size*d4_size*element_size;
    char * accel_row = (char*)accel_address;

    // Will transfert "d1_block_size" rows
    for(size_t i = 0; i < d1_block_size; i++) {
      P4A_copy_to_accel_3d(element_size,
                           d2_size,d3_size,d4_size,
                           d2_block_size, d3_block_size, d4_block_size,
                           d2_offset, d3_offset, d4_offset,
                           host_row,
                           accel_row);

      // Comput addresses for next row
      host_row += d4_size*element_size;
      accel_row += d4_block_size*element_size;
    }
  }
}

#endif // P4A_ACCEL_CL
