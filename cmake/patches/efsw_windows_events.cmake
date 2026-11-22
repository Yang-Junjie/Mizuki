# efsw 1.7.3 compares second-resolution mtimes and sizes when deduplicating
# Windows writes. Forward modifications to Mizuki's existing event coalescing
# instead, so distinct saves of the same size within one second are preserved.
set(efsw_windows_source "${efsw_SOURCE_DIR}/src/efsw/WatcherWin32.cpp")
file(READ "${efsw_windows_source}" efsw_windows_original)
string(REPLACE "\r\n" "\n" efsw_windows_patched "${efsw_windows_original}")
set(efsw_duplicate_body [=[
			FileInfo fifile( std::string( pWatch->DirName ) + nfile );

			if ( pWatch->LastModifiedEvent.file.ModificationTime == fifile.ModificationTime &&
				 pWatch->LastModifiedEvent.file.Size == fifile.Size &&
				 pWatch->LastModifiedEvent.fileName == nfile ) {
				skip = true;
			}

			pWatch->LastModifiedEvent.fileName = nfile;
			pWatch->LastModifiedEvent.file = fifile;
]=])
foreach(action "pNotify->Action" "event.Action")
    set(duplicate_filter "if ( FILE_ACTION_MODIFIED == ${action} ) {\n${efsw_duplicate_body}\t\t}")
    set(replacement "// Mizuki coalesces notifications without discarding same-size writes.")
    if(action STREQUAL "event.Action")
        string(APPEND duplicate_filter " else ")
        set(replacement "")
    endif()
    string(REPLACE "${duplicate_filter}" "${replacement}"
        efsw_windows_patched "${efsw_windows_patched}")
endforeach()
if(efsw_windows_patched MATCHES "LastModifiedEvent")
    message(FATAL_ERROR "Cannot remove efsw's Windows modification filter; review the pinned dependency")
endif()
if(NOT efsw_windows_patched STREQUAL efsw_windows_original)
    file(WRITE "${efsw_windows_source}" "${efsw_windows_patched}")
endif()
