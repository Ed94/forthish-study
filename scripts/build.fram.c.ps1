$path_root = split-path -Path $PSScriptRoot -Parent
$misc = join-path $PSScriptRoot 'helpers/misc.ps1'
. $misc

$path_toolchain = join-path $path_root      'toolchain'
$path_rad       = join-path $path_toolchain 'rad'
# --- Toolchain Executable Paths ---
$compiler  = 'clang'
$optimizer = 'opt.exe'
$linker    = 'lld-link.exe'
$archiver  = 'llvm-lib.exe'
$radbin    = join-path $path_rad 'radbin.exe'
$radlink   = join-path $path_rad 'radlink.exe'

# https://clang.llvm.org/docs/ClangCommandLineReference.html
$flag_all_c 					   = @('-x', 'c')
$flag_c11                          = '-std=c11'
$flag_c23                          = '-std=c23'
$flag_all_cpp                      = '-x c++'
$flag_charset_utf8                 = '-fexec-charset=utf-8'
$flag_compile                      = '-c'
$flag_color_diagnostics            = '-fcolor-diagnostics'
$flag_no_builtin_includes          = '-nobuiltininc'
$flag_no_color_diagnostics         = '-fno-color-diagnostics'
$flag_debug                        = '-g'
$flag_debug_codeview               = '-gcodeview'
$flag_define                       = '-D'
$flag_emit_llvm                    = '-emit-llvm'
$flag_stop_after_gen               = '-S'
$flag_exceptions_disabled		   = '-fno-exceptions'
$flag_rtti_disabled                = '-fno-rtti'
$flag_diagnostics_absolute_paths   = '-fdiagnostics-absolute-paths'
$flag_preprocess 			       = '-E'
$flag_include                      = '-I'
$flag_section_data                 = '-fdata-sections'
$flag_section_functions            = '-ffunction-sections'
$flag_library					   = '-l'
$flag_library_path				   = '-L'
$flag_linker                       = '-Wl,'
$flag_link_dll                     = '/DLL'
$flag_link_mapfile 		           = '/MAP:'
$flag_link_optimize_references     = '/OPT:REF'
$flag_link_win_subsystem_console   = '/SUBSYSTEM:CONSOLE'
$flag_link_win_subsystem_windows   = '/SUBSYSTEM:WINDOWS'
$flag_link_win_machine_32          = '/MACHINE:X86'
$flag_link_win_machine_64          = '/MACHINE:X64'
$flag_link_win_debug               = '/DEBUG'
$flag_link_win_pdb 			       = '/PDB:'
$flag_link_win_path_output         = '/OUT:'
$flag_link_no_incremental          = '/INCREMENTAL:NO'
$flag_no_optimization 		       = '-O0'
$flag_optimize_fast 		       = '-O2'
$flag_optimize_size 		       = '-O1'
$flag_optimize_intrinsics		   = '-Oi'
$flag_path_output                  = '-o'
$flag_preprocess_non_intergrated   = '-no-integrated-cpp'
$flag_profiling_debug              = '-fdebug-info-for-profiling'
$flag_set_stack_size			   = '-stack='
$flag_syntax_only				   = '-fsyntax-only'
$flag_target_arch				   = '-target'
$flag_time_trace				   = '-ftime-trace'
$flag_verbose 					   = '-v'
$flag_wall 					       = '-Wall'
$flag_warning 					   = '-W'
$flag_warnings_as_errors           = '-Werror'
$flag_nologo 			           = '/nologo'

$path_build = join-path $path_root 'build'
if ( -not(test-path -Path $path_build) ) {
	new-item -ItemType Directory -Path $path_build
}

push-location $path_build

# --- File Paths ---
$unit_name      = "fram"
$unit_source    = join-path $path_root "code\C\$unit_name.c"
$ir_unoptimized = join-path $path_build "$unit_name.ll"
$ir_optimized   = join-path $path_build "$unit_name.opt.ll"
$object         = join-path $path_build "$unit_name.obj"
$binary         = join-path $path_build "$unit_name.exe"
$pdb            = join-path $path_build "$unit_name.pdb"
$map            = join-path $path_build "$unit_name.map"

# --- Stage 1: Compile C to LLVM IR ---
write-host "Stage 1: Compiling C to LLVM IR"
$compiler_args = @()
# $compiler_args += $flag_stop_after_gen
# $compiler_args += $flag_emit_llvm
$compiler_args += ($flag_define + 'BUILD_DEBUG=1')
$compiler_args += $flag_debug
# $compiler_args += $flag_debug_codeview
$compiler_args += $flag_wall
# $compiler_args += $flag_charset_utf8
$compiler_args += $flag_c23
$compiler_args += $flag_no_optimization
# $compiler_args += $flag_no_builtin_includes
$compiler_args += $flag_diagnostics_absolute_paths
$compiler_args += $flag_rtti_disabled
$compiler_args += $flag_exceptions_disabled
$compiler_args += ($flag_include + $path_root)
$compiler_args += $flag_compile
$compiler_args += $flag_path_output, $object
$compiler_args += $unit_source
$compiler_args | ForEach-Object { Write-Host $_ }
$stage1_time = Measure-Command { & $compiler $compiler_args }
write-host "Compilation took $($stage1_time.TotalMilliseconds)ms"
# write-host "IR generation took $($stage1_time.TotalMilliseconds)ms"
write-host

# --- Stage 2: Manually Optimize LLVM IR ---
if ($false) {
write-host "Manually Optimizing LLVM IR with 'opt'"
$optimization_passes = @(
    '-sroa',              # Scalar Replacement Of Aggregates
    '-early-cse',         # Early Common Subexpression Elimination
    '-instcombine'        # Instruction Combining
)
$optimizer_args = @(
	$optimization_passes, 
	$ir_unoptimized, 
	$flag_path_output, 
	$ir_optimized
)
$optimizer_args | ForEach-Object { Write-Host $_ }
$stage2_time = Measure-Command { & $optimizer $optimizer_args }
write-host "Optimization took $($stage2_time.TotalMilliseconds)ms"
write-hosts

write-host "Compiling LLVM IR to Object File with 'clang'"
$ir_to_obj_args = @()
$ir_to_obj_args += $flag_compile
$ir_to_obj_args += $flag_path_output, $object
$ir_to_obj_args += $ir_optimized

$ir_to_obj_args | ForEach-Object { Write-Host $_ }
$stage3_time = Measure-Command { & $compiler $ir_to_obj_args }
write-host "Object file generation took $($stage3_time.TotalMilliseconds)ms"
write-host
}
if ($true) {
	# write-host "Linking with lld-link"
	$linker_args = @()
	$linker_args += $flag_nologo
	$linker_args += $flag_link_win_machine_64
	$linker_args += $flag_link_no_incremental
	$linker_args += ($flag_link_win_path_output + $binary)

	$linker_args += "$flag_link_win_debug"
	$linker_args += $flag_link_win_pdb + $pdb
	$linker_args += $flag_link_mapfile + $map
    $linker_args += $flag_link_win_subsystem_console

	$linker_args += $object
    
	# Diagnoistc print for the args
	$linker_args | ForEach-Object { Write-Host $_ }

	$linking_time = Measure-Command { & $linker $linker_args }
	write-host "Linking took $($linking_time.TotalMilliseconds)ms"
	write-host
}
if ($false) {
	write-host "Dumping Debug Info"
	$rbin_out  = '--out:'
	$rbin_dump = '--dump'
    $rdi         = join-path $path_build "$unit_name.rdi"
    $rdi_listing = join-path $path_build "$unit_name.rdi.list"

	$nargs = @($pdb, ($rbin_out + $rdi))
	& $radbin $nargs

	$nargs = @($rbin_dump, $rdi)
	$dump = & $radbin $nargs
	$dump > $rdi_listing
}

Pop-Location
