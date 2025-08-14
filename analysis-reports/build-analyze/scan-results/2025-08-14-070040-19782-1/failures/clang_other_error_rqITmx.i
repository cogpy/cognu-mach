# 1 "vm/memory_object_user.user.defs.c"
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 367 "<built-in>" 3
# 1 "<command line>" 1
# 1 "<built-in>" 2
# 1 "./config.h" 1
# 2 "<built-in>" 2
# 1 "vm/memory_object_user.user.defs.c" 2
# 28 "vm/memory_object_user.user.defs.c"
# 1 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs" 1
# 33 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
subsystem

   KernelUser




       memory_object 2200;

# 1 "/home/runner/work/cognu-mach/cognu-mach/include/mach/std_types.defs" 1
# 33 "/home/runner/work/cognu-mach/cognu-mach/include/mach/std_types.defs"
type int32_t = MACH_MSG_TYPE_INTEGER_32;
type int64_t = MACH_MSG_TYPE_INTEGER_64;
type boolean_t = MACH_MSG_TYPE_BOOLEAN;
type unsigned = MACH_MSG_TYPE_INTEGER_32;
type uint32_t = MACH_MSG_TYPE_INTEGER_32;
type uint64_t = MACH_MSG_TYPE_INTEGER_64;


# 1 "./mach/machine/machine_types.defs" 1
# 46 "./mach/machine/machine_types.defs"
type natural_t = uint32_t;
# 55 "./mach/machine/machine_types.defs"
type integer_t = int32_t;
# 69 "./mach/machine/machine_types.defs"
type rpc_long_natural_t = uint32_t;
type rpc_long_integer_t = int32_t;
# 79 "./mach/machine/machine_types.defs"
type long_natural_t = rpc_long_natural_t




    ctype: rpc_long_natural_t

    ;





type long_integer_t = rpc_long_integer_t




    ctype: rpc_long_integer_t

    ;




type rpc_phys_addr_t = uint64_t;
type rpc_phys_addr_array_t = array[] of rpc_phys_addr_t;
# 42 "/home/runner/work/cognu-mach/cognu-mach/include/mach/std_types.defs" 2

type kern_return_t = int;

type pointer_t = ^array[] of MACH_MSG_TYPE_BYTE
 ctype: vm_offset_t;


type mach_port_t = MACH_MSG_TYPE_COPY_SEND





;
type mach_port_array_t = array[] of mach_port_t;

type mach_port_name_t = MACH_MSG_TYPE_PORT_NAME;
type mach_port_name_array_t = array[] of mach_port_name_t;

type mach_port_right_t = natural_t;

type mach_port_type_t = natural_t;
type mach_port_type_array_t = array[] of mach_port_type_t;

type mach_port_urefs_t = natural_t;
type mach_port_delta_t = integer_t;
type mach_port_seqno_t = natural_t;
type mach_port_mscount_t = unsigned;
type mach_port_msgcount_t = unsigned;
type mach_port_rights_t = unsigned;
type mach_msg_id_t = integer_t;
type mach_msg_type_name_t = unsigned;
type mach_msg_type_number_t = natural_t;

type mach_port_move_receive_t = MACH_MSG_TYPE_MOVE_RECEIVE
 ctype: mach_port_t;
type mach_port_copy_send_t = MACH_MSG_TYPE_COPY_SEND
 ctype: mach_port_t;
type mach_port_make_send_t = MACH_MSG_TYPE_MAKE_SEND
 ctype: mach_port_t;
type mach_port_move_send_t = MACH_MSG_TYPE_MOVE_SEND
 ctype: mach_port_t;
type mach_port_make_send_once_t = MACH_MSG_TYPE_MAKE_SEND_ONCE
 ctype: mach_port_t;
type mach_port_move_send_once_t = MACH_MSG_TYPE_MOVE_SEND_ONCE
 ctype: mach_port_t;

type mach_port_receive_t = MACH_MSG_TYPE_PORT_RECEIVE
 ctype: mach_port_t;
type mach_port_send_t = MACH_MSG_TYPE_PORT_SEND
 ctype: mach_port_t;
type mach_port_send_once_t = MACH_MSG_TYPE_PORT_SEND_ONCE
 ctype: mach_port_t;

type mach_port_poly_t = polymorphic
 ctype: mach_port_t;

import <mach/std_types.h>;
# 43 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs" 2
# 1 "/home/runner/work/cognu-mach/cognu-mach/include/mach/mach_types.defs" 1
# 61 "/home/runner/work/cognu-mach/cognu-mach/include/mach/mach_types.defs"
type mach_port_status_t = struct {
   mach_port_name_t mps_pset;
   mach_port_seqno_t mps_seqno;
   mach_port_mscount_t mps_mscount;
   mach_port_msgcount_t mps_qlimit;
   mach_port_msgcount_t mps_msgcount;
   mach_port_rights_t mps_sorights;
   boolean_t mps_srights;
   boolean_t mps_pdrequest;
   boolean_t mps_nsrequest;
};

type task_t = mach_port_t
  ctype: mach_port_t





  ;
# 93 "/home/runner/work/cognu-mach/cognu-mach/include/mach/mach_types.defs"
type thread_t = mach_port_t
  ctype: mach_port_t





  ;

type thread_state_t = array[*:1024] of natural_t;

type task_array_t = ^array[] of task_t;
type thread_array_t = ^array[] of thread_t;

type vm_task_t = mach_port_t
  ctype: mach_port_t




  ;

type ipc_space_t = mach_port_t
  ctype: mach_port_t




  ;





type rpc_uintptr_t = uint64_t;
type rpc_vm_size_t = uint64_t;


 type rpc_vm_offset_t = rpc_vm_size_t;

type vm_address_t = rpc_vm_size_t




    ctype: uint64_t

    ;
type vm_offset_t = rpc_vm_offset_t




    ctype: uint64_t

    ;
type vm_size_t = rpc_vm_size_t




    ctype: uint64_t

;
type vm_prot_t = int;
type vm_inherit_t = int;
type vm_statistics_data_t = struct[13] of integer_t;
type vm_machine_attribute_t = int;
type vm_machine_attribute_val_t = int;
type vm_sync_t = int;

type thread_info_t = array[*:1024] of integer_t;

type task_info_t = array[*:1024] of integer_t;

type memory_object_t = mach_port_t
  ctype: mach_port_t
# 186 "/home/runner/work/cognu-mach/cognu-mach/include/mach/mach_types.defs"
  ;

type memory_object_control_t = mach_port_t
  ctype: mach_port_t



  ;

type memory_object_name_t = mach_port_t
  ctype: mach_port_t




  ;

type memory_object_copy_strategy_t = int;
type memory_object_return_t = int;

type host_t = mach_port_t
  ctype: mach_port_t




  ;

type host_priv_t = mach_port_t
  ctype: mach_port_t



  ;

type host_info_t = array[*:1024] of integer_t;

type processor_t = mach_port_t
  ctype: mach_port_t




  ;

type processor_array_t = ^array[] of processor_t;
type processor_info_t = array[*:1024] of integer_t;

type processor_set_t = mach_port_t
  ctype: mach_port_t





  ;

type processor_set_array_t = ^array[] of processor_set_t;

type processor_set_name_t = mach_port_t
  ctype: mach_port_t





  ;

type processor_set_name_array_t = ^array[] of processor_set_name_t;

type processor_set_info_t = array[*:1024] of integer_t;

type kernel_version_t = (MACH_MSG_TYPE_STRING, 512*8);
type new_kernel_version_t = c_string[512]
  ctype: kernel_version_t;

type rpc_time_value_t = struct {
  rpc_long_integer_t seconds;
  integer_t microseconds;
};
type time_value_t = rpc_time_value_t




    ctype: rpc_time_value_t

    ;

type time_value64_t = struct {
  int64_t seconds;
  int64_t nanoseconds;
};

type emulation_vector_t = ^array[] of vm_offset_t;

type rpc_signature_info_t = array[*:1024] of int;
# 297 "/home/runner/work/cognu-mach/cognu-mach/include/mach/mach_types.defs"
import <mach/mach_types.h>;
# 44 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs" 2






serverprefix seqnos_;
serverdemux seqnos_memory_object_server;
# 65 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
simpleroutine memory_object_init(
  memory_object : memory_object_t;

 msgseqno seqno : mach_port_seqno_t;

  memory_control : memory_object_control_t =
      MACH_MSG_TYPE_MAKE_SEND
      ctype: mach_port_t;
  memory_object_name : memory_object_name_t =
      MACH_MSG_TYPE_MAKE_SEND
      ctype: mach_port_t;
  memory_object_page_size : vm_size_t);
# 90 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
simpleroutine memory_object_terminate(
  memory_object : memory_object_t =
      MACH_MSG_TYPE_MOVE_SEND
      ctype: mach_port_t
# 104 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
      ;

 msgseqno seqno : mach_port_seqno_t;

  memory_control : memory_object_control_t =
      MACH_MSG_TYPE_MOVE_RECEIVE
      ctype: mach_port_t


      , dealloc

      ;
  memory_object_name : memory_object_name_t =
      MACH_MSG_TYPE_MOVE_RECEIVE
      ctype: mach_port_t


      , dealloc

      );
# 152 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
simpleroutine memory_object_copy(
  old_memory_object : memory_object_t;

 msgseqno seqno : mach_port_seqno_t;

  old_memory_control : memory_object_control_t =
      MACH_MSG_TYPE_MAKE_SEND
      ctype: mach_port_t;
  offset : vm_offset_t;
  length : vm_size_t;
  new_memory_object : memory_object_t =
      MACH_MSG_TYPE_MOVE_RECEIVE
      ctype: mach_port_t


      , dealloc

      );
# 178 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
simpleroutine memory_object_data_request(
  memory_object : memory_object_t;

 msgseqno seqno : mach_port_seqno_t;

  memory_control : memory_object_control_t =
      MACH_MSG_TYPE_MAKE_SEND
      ctype: mach_port_t;
  offset : vm_offset_t;
  length : vm_size_t;
  desired_access : vm_prot_t);
# 197 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
simpleroutine memory_object_data_unlock(
  memory_object : memory_object_t;

 msgseqno seqno : mach_port_seqno_t;

  memory_control : memory_object_control_t =
      MACH_MSG_TYPE_MAKE_SEND
      ctype: mach_port_t;
  offset : vm_offset_t;
  length : vm_size_t;
  desired_access : vm_prot_t);

skip;
# 219 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
simpleroutine memory_object_lock_completed(
  memory_object : memory_object_t =
   polymorphic|MACH_MSG_TYPE_PORT_SEND_ONCE
   ctype: mach_port_t
# 232 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
   ;

 msgseqno seqno : mach_port_seqno_t;

  memory_control : memory_object_control_t =
      MACH_MSG_TYPE_MAKE_SEND
      ctype: mach_port_t;
  offset : vm_offset_t;
  length : vm_size_t);
# 260 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
simpleroutine memory_object_supply_completed(
  memory_object : memory_object_t =
   polymorphic|MACH_MSG_TYPE_PORT_SEND_ONCE
   ctype: mach_port_t
# 273 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
   ;

 msgseqno seqno : mach_port_seqno_t;

  memory_control : memory_object_control_t =
      MACH_MSG_TYPE_MAKE_SEND
      ctype: mach_port_t;
  offset : vm_offset_t;
  length : vm_size_t;
  result : kern_return_t;
  error_offset : vm_offset_t);
# 295 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
simpleroutine memory_object_data_return(
  memory_object : memory_object_t;

 msgseqno seqno : mach_port_seqno_t;

  memory_control : memory_object_control_t =
      MACH_MSG_TYPE_MAKE_SEND
      ctype: mach_port_t;
  offset : vm_offset_t;
  data : pointer_t;
  dirty : boolean_t;
  kernel_copy : boolean_t);
# 315 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
simpleroutine memory_object_change_completed(
  memory_object : memory_object_t =
   polymorphic|MACH_MSG_TYPE_PORT_SEND_ONCE
   ctype: mach_port_t
# 328 "/home/runner/work/cognu-mach/cognu-mach/include/mach/memory_object.defs"
   ;

 msgseqno seqno : mach_port_seqno_t;

  may_cache : boolean_t;
  copy_strategy : memory_object_copy_strategy_t);
# 29 "vm/memory_object_user.user.defs.c" 2

