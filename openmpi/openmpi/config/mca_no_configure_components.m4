dnl
dnl $HEADER
dnl

dnl This file is automatically created by autogen.sh; it should not
dnl be edited by hand!!

m4_define(mca_backtrace_no_config_component_list, [])
m4_define(mca_backtrace_m4_config_component_list, [execinfo, printstack, darwin, none])
m4_define(mca_installdirs_no_config_component_list, [])
m4_define(mca_installdirs_m4_config_component_list, [env, config])
m4_define(mca_maffinity_no_config_component_list, [first_use])
m4_define(mca_maffinity_m4_config_component_list, [libnuma])
m4_define(mca_memcpy_no_config_component_list, [])
m4_define(mca_memcpy_m4_config_component_list, [])
m4_define(mca_memory_no_config_component_list, [])
m4_define(mca_memory_m4_config_component_list, [darwin, ptmalloc2, malloc_hooks])
m4_define(mca_paffinity_no_config_component_list, [])
m4_define(mca_paffinity_m4_config_component_list, [linux, solaris, windows])
m4_define(mca_timer_no_config_component_list, [])
m4_define(mca_timer_m4_config_component_list, [aix, altix, darwin, solaris, windows, linux])
m4_define(mca_opal_framework_list, [backtrace, installdirs, maffinity, memcpy, memory, paffinity, timer])
m4_define(mca_errmgr_no_config_component_list, [hnp, orted, proxy])
m4_define(mca_errmgr_m4_config_component_list, [bproc])
m4_define(mca_gpr_no_config_component_list, [null, proxy, replica])
m4_define(mca_gpr_m4_config_component_list, [])
m4_define(mca_iof_no_config_component_list, [proxy, svc])
m4_define(mca_iof_m4_config_component_list, [])
m4_define(mca_ns_no_config_component_list, [proxy, replica])
m4_define(mca_ns_m4_config_component_list, [])
m4_define(mca_odls_no_config_component_list, [])
m4_define(mca_odls_m4_config_component_list, [bproc, default])
m4_define(mca_oob_no_config_component_list, [])
m4_define(mca_oob_m4_config_component_list, [tcp])
m4_define(mca_pls_no_config_component_list, [proxy])
m4_define(mca_pls_m4_config_component_list, [bproc, cnos, gridengine, poe, rsh, slurm, tm, xgrid])
m4_define(mca_ras_no_config_component_list, [dash_host, localhost])
m4_define(mca_ras_m4_config_component_list, [bjs, gridengine, loadleveler, lsf_bproc, slurm, tm, xgrid])
m4_define(mca_rds_no_config_component_list, [hostfile, proxy, resfile])
m4_define(mca_rds_m4_config_component_list, [])
m4_define(mca_rmaps_no_config_component_list, [round_robin])
m4_define(mca_rmaps_m4_config_component_list, [])
m4_define(mca_rmgr_no_config_component_list, [proxy, urm])
m4_define(mca_rmgr_m4_config_component_list, [cnos])
m4_define(mca_rml_no_config_component_list, [oob])
m4_define(mca_rml_m4_config_component_list, [cnos])
m4_define(mca_schema_no_config_component_list, [])
m4_define(mca_schema_m4_config_component_list, [])
m4_define(mca_sds_no_config_component_list, [env, seed, singleton])
m4_define(mca_sds_m4_config_component_list, [bproc, cnos, pipe, portals_utcp, slurm])
m4_define(mca_smr_no_config_component_list, [])
m4_define(mca_smr_m4_config_component_list, [bproc])
m4_define(mca_orte_framework_list, [errmgr, gpr, iof, ns, odls, oob, pls, ras, rds, rmaps, rmgr, rml, schema, sds, smr])
m4_define(mca_allocator_no_config_component_list, [basic, bucket])
m4_define(mca_allocator_m4_config_component_list, [])
m4_define(mca_bml_no_config_component_list, [r2])
m4_define(mca_bml_m4_config_component_list, [])
m4_define(mca_btl_no_config_component_list, [self, sm])
m4_define(mca_btl_m4_config_component_list, [gm, mvapi, mx, openib, portals, tcp, udapl])
m4_define(mca_coll_no_config_component_list, [basic, self, sm, tuned])
m4_define(mca_coll_m4_config_component_list, [])
m4_define(mca_common_no_config_component_list, [sm])
m4_define(mca_common_m4_config_component_list, [mx, portals])
m4_define(mca_io_no_config_component_list, [])
m4_define(mca_io_m4_config_component_list, [romio])
m4_define(mca_mpool_no_config_component_list, [rdma, sm])
m4_define(mca_mpool_m4_config_component_list, [])
m4_define(mca_mtl_no_config_component_list, [])
m4_define(mca_mtl_m4_config_component_list, [mx, portals, psm])
m4_define(mca_osc_no_config_component_list, [pt2pt])
m4_define(mca_osc_m4_config_component_list, [])
m4_define(mca_pml_no_config_component_list, [cm, ob1])
m4_define(mca_pml_m4_config_component_list, [])
m4_define(mca_rcache_no_config_component_list, [vma])
m4_define(mca_rcache_m4_config_component_list, [])
m4_define(mca_topo_no_config_component_list, [unity])
m4_define(mca_topo_m4_config_component_list, [])
m4_define(mca_ompi_framework_list, [allocator, bml, btl, coll, common, io, mpool, mtl, osc, pml, rcache, topo])
m4_define(mca_project_list, [opal, orte, ompi])

dnl List all the no-configure components that we found, and AC_DEFINE
dnl their versions

AC_DEFUN([MCA_NO_CONFIG_CONFIG_FILES],[

dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/backtrace/darwin

AC_CONFIG_FILES([opal/mca/backtrace/darwin/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/backtrace/execinfo

AC_CONFIG_FILES([opal/mca/backtrace/execinfo/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/backtrace/none

AC_CONFIG_FILES([opal/mca/backtrace/none/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/backtrace/printstack

AC_CONFIG_FILES([opal/mca/backtrace/printstack/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/installdirs/config

AC_CONFIG_FILES([opal/mca/installdirs/config/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/installdirs/env

AC_CONFIG_FILES([opal/mca/installdirs/env/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    opal/mca/maffinity/first_use

AC_CONFIG_FILES([opal/mca/maffinity/first_use/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/maffinity/libnuma

AC_CONFIG_FILES([opal/mca/maffinity/libnuma/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/memory/darwin

AC_CONFIG_FILES([opal/mca/memory/darwin/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/memory/malloc_hooks

AC_CONFIG_FILES([opal/mca/memory/malloc_hooks/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/memory/ptmalloc2

AC_CONFIG_FILES([opal/mca/memory/ptmalloc2/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/paffinity/linux

AC_CONFIG_FILES([opal/mca/paffinity/linux/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/paffinity/solaris

AC_CONFIG_FILES([opal/mca/paffinity/solaris/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/paffinity/windows

AC_CONFIG_FILES([opal/mca/paffinity/windows/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/timer/aix

AC_CONFIG_FILES([opal/mca/timer/aix/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/timer/altix

AC_CONFIG_FILES([opal/mca/timer/altix/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/timer/darwin

AC_CONFIG_FILES([opal/mca/timer/darwin/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/timer/linux

AC_CONFIG_FILES([opal/mca/timer/linux/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/timer/solaris

AC_CONFIG_FILES([opal/mca/timer/solaris/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    opal/mca/timer/windows

AC_CONFIG_FILES([opal/mca/timer/windows/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/errmgr/bproc

AC_CONFIG_FILES([orte/mca/errmgr/bproc/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/errmgr/hnp

AC_CONFIG_FILES([orte/mca/errmgr/hnp/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/errmgr/orted

AC_CONFIG_FILES([orte/mca/errmgr/orted/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/errmgr/proxy

AC_CONFIG_FILES([orte/mca/errmgr/proxy/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/gpr/null

AC_CONFIG_FILES([orte/mca/gpr/null/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/gpr/proxy

AC_CONFIG_FILES([orte/mca/gpr/proxy/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/gpr/replica

AC_CONFIG_FILES([orte/mca/gpr/replica/Makefile])
AC_CONFIG_FILES([orte/mca/gpr/replica/api_layer/Makefile])
AC_CONFIG_FILES([orte/mca/gpr/replica/transition_layer/Makefile])
AC_CONFIG_FILES([orte/mca/gpr/replica/functional_layer/Makefile])
AC_CONFIG_FILES([orte/mca/gpr/replica/communications/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/iof/proxy

AC_CONFIG_FILES([orte/mca/iof/proxy/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/iof/svc

AC_CONFIG_FILES([orte/mca/iof/svc/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/ns/proxy

AC_CONFIG_FILES([orte/mca/ns/proxy/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/ns/replica

AC_CONFIG_FILES([orte/mca/ns/replica/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/odls/bproc

AC_CONFIG_FILES([orte/mca/odls/bproc/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/odls/default

AC_CONFIG_FILES([orte/mca/odls/default/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/oob/tcp

AC_CONFIG_FILES([orte/mca/oob/tcp/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/pls/bproc

AC_CONFIG_FILES([orte/mca/pls/bproc/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/pls/cnos

AC_CONFIG_FILES([orte/mca/pls/cnos/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/pls/gridengine

AC_CONFIG_FILES([orte/mca/pls/gridengine/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/pls/poe

AC_CONFIG_FILES([orte/mca/pls/poe/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/pls/proxy

AC_CONFIG_FILES([orte/mca/pls/proxy/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/pls/rsh

AC_CONFIG_FILES([orte/mca/pls/rsh/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/pls/slurm

AC_CONFIG_FILES([orte/mca/pls/slurm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/pls/tm

AC_CONFIG_FILES([orte/mca/pls/tm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/pls/xgrid

AC_CONFIG_FILES([orte/mca/pls/xgrid/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/ras/bjs

AC_CONFIG_FILES([orte/mca/ras/bjs/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/ras/dash_host

AC_CONFIG_FILES([orte/mca/ras/dash_host/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/ras/gridengine

AC_CONFIG_FILES([orte/mca/ras/gridengine/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/ras/loadleveler

AC_CONFIG_FILES([orte/mca/ras/loadleveler/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/ras/localhost

AC_CONFIG_FILES([orte/mca/ras/localhost/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/ras/lsf_bproc

AC_CONFIG_FILES([orte/mca/ras/lsf_bproc/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/ras/slurm

AC_CONFIG_FILES([orte/mca/ras/slurm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/ras/tm

AC_CONFIG_FILES([orte/mca/ras/tm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/ras/xgrid

AC_CONFIG_FILES([orte/mca/ras/xgrid/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/rds/hostfile

AC_CONFIG_FILES([orte/mca/rds/hostfile/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/rds/proxy

AC_CONFIG_FILES([orte/mca/rds/proxy/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/rds/resfile

AC_CONFIG_FILES([orte/mca/rds/resfile/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/rmaps/round_robin

AC_CONFIG_FILES([orte/mca/rmaps/round_robin/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/rmgr/cnos

AC_CONFIG_FILES([orte/mca/rmgr/cnos/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/rmgr/proxy

AC_CONFIG_FILES([orte/mca/rmgr/proxy/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/rmgr/urm

AC_CONFIG_FILES([orte/mca/rmgr/urm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/rml/cnos

AC_CONFIG_FILES([orte/mca/rml/cnos/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/rml/oob

AC_CONFIG_FILES([orte/mca/rml/oob/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/sds/bproc

AC_CONFIG_FILES([orte/mca/sds/bproc/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/sds/cnos

AC_CONFIG_FILES([orte/mca/sds/cnos/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/sds/env

AC_CONFIG_FILES([orte/mca/sds/env/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/sds/pipe

AC_CONFIG_FILES([orte/mca/sds/pipe/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/sds/portals_utcp

AC_CONFIG_FILES([orte/mca/sds/portals_utcp/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/sds/seed

AC_CONFIG_FILES([orte/mca/sds/seed/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    orte/mca/sds/singleton

AC_CONFIG_FILES([orte/mca/sds/singleton/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/sds/slurm

AC_CONFIG_FILES([orte/mca/sds/slurm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    orte/mca/smr/bproc

AC_CONFIG_FILES([orte/mca/smr/bproc/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/allocator/basic

AC_CONFIG_FILES([ompi/mca/allocator/basic/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/allocator/bucket

AC_CONFIG_FILES([ompi/mca/allocator/bucket/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/bml/r2

AC_CONFIG_FILES([ompi/mca/bml/r2/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/btl/gm

AC_CONFIG_FILES([ompi/mca/btl/gm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/btl/mvapi

AC_CONFIG_FILES([ompi/mca/btl/mvapi/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/btl/mx

AC_CONFIG_FILES([ompi/mca/btl/mx/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/btl/openib

AC_CONFIG_FILES([ompi/mca/btl/openib/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/btl/portals

AC_CONFIG_FILES([ompi/mca/btl/portals/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/btl/self

AC_CONFIG_FILES([ompi/mca/btl/self/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/btl/sm

AC_CONFIG_FILES([ompi/mca/btl/sm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/btl/tcp

AC_CONFIG_FILES([ompi/mca/btl/tcp/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/btl/udapl

AC_CONFIG_FILES([ompi/mca/btl/udapl/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/coll/basic

AC_CONFIG_FILES([ompi/mca/coll/basic/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/coll/self

AC_CONFIG_FILES([ompi/mca/coll/self/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/coll/sm

AC_CONFIG_FILES([ompi/mca/coll/sm/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/coll/tuned

AC_CONFIG_FILES([ompi/mca/coll/tuned/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/common/mx

AC_CONFIG_FILES([ompi/mca/common/mx/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/common/portals

AC_CONFIG_FILES([ompi/mca/common/portals/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/common/sm

AC_CONFIG_FILES([ompi/mca/common/sm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/io/romio

AC_CONFIG_FILES([ompi/mca/io/romio/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/mpool/rdma

AC_CONFIG_FILES([ompi/mca/mpool/rdma/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/mpool/sm

AC_CONFIG_FILES([ompi/mca/mpool/sm/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/mtl/mx

AC_CONFIG_FILES([ompi/mca/mtl/mx/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/mtl/portals

AC_CONFIG_FILES([ompi/mca/mtl/portals/Makefile])
dnl ----------------------------------------------------------------

dnl m4-configure component: 
dnl    ompi/mca/mtl/psm

AC_CONFIG_FILES([ompi/mca/mtl/psm/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/osc/pt2pt

AC_CONFIG_FILES([ompi/mca/osc/pt2pt/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/pml/cm

AC_CONFIG_FILES([ompi/mca/pml/cm/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/pml/ob1

AC_CONFIG_FILES([ompi/mca/pml/ob1/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/rcache/vma

AC_CONFIG_FILES([ompi/mca/rcache/vma/Makefile])
dnl ----------------------------------------------------------------

dnl No-configure component: 
dnl    ompi/mca/topo/unity

AC_CONFIG_FILES([ompi/mca/topo/unity/Makefile])
])dnl
