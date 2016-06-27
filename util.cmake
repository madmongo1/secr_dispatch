macro (propagate)
	foreach (_var ${ARGN})
		list(APPEND to_propagate "${_var}")
	endforeach ()
	
	if (to_propagate)
		list (REMOVE_DUPLICATES to_propagate)
	endif ()

	foreach (_var IN LISTS to_propagate)
		set (${_var} "${${_var}}" PARENT_SCOPE)
	endforeach ()
	set (to_propagate "${to_propagate}" PARENT_SCOPE)
endmacro ()


macro (dump_propagated)
	message (STATUS "${to_propagate}")
	foreach (_list IN LISTS to_propagate)
		message (STATUS "${_list} = ${${_list}}")
	endforeach ()
endmacro ()
