#! /bin/sh

while getopts 'cvs:m:' opt
do
	case $opt in
	c)
		export GPM_OUTPUT_STYLE=C;;
	s)
		eval export GPM_OPRSET_NAME=$OPTARG;;
	m)
		eval export GPM_OPRMX_NAME=$OPTARG;;
	v)
		export GPM_VERBOSE=1;;
	esac
done

shift $((OPTIND-1))

if [ ! -f "$1" ]; then
	echo "No such file: $1" >&2
	exit 1
fi

gawk -v VERBOSE=1 -- '
func xset_add(set,elem, __ARGVEND__,i,n,a,inside)
{
	n = split(set, a)
	for(i = 1; i <= n; i++)
		if(a[i] == elem)
			inside = 1
	if( ! inside ) {
		a[n + 1] = elem
		asort(a)
		set = ""
		for(i = 1; i <= n + 1; i++) {
			set = set " " a[i]
		}
	}
	return set
}

func xset_join(set1,set2, __ARGVEND__,i,n,a)
{
	n = split(set2, a)
	for(i = 1; i <= n; i++)
		set1 = xset_add(set1, a[i])
	return set1
}


func is_non_terminator(s) {
	return s ~ /^[a-z_][a-z_]*$/ 
}

func is_terminator(s)
{
	if(s ~ /^'"'.+'"'$/)
		return 1
	return 0
}

func get_operator(s, __ARGVEND__, a) {
	if(s == "'"'int_const'"'")
		return "i"
	if( s !~ /^'"'.+'"'$/ ) {
		return "";
	}
	split(s, a, "'\''");
#	printf("$x = %s, opr = %s\n", s, a[2]) > STDERR
	return a[2]
}

BEGIN {
	if( ENVIRON["GPM_VERBOSE"] ) {
		STDERR = "/dev/stderr"
		STDOUT = "/dev/stderr"
	} else {
		STDERR = "/dev/null"
		STDOUT = "/dev/null"
	}
	RED_FONT = "\033[31m%s\033[0m"

	print "[0] " ARGV[1] >STDERR
	print "[1] " ARGV[2] >STDERR
}

func get_schemes(__ARGV__, i, a)
{
	__NF = split(current_string, dollar)
	for(i = 3; i <= __NF; i++) {
		if( !is_terminator(dollar[i]) ) {
			delete is_simple[current_nt]
			break
		}
	}
	if( current_nt in is_simple ) {
		split(current_string, a, ":")
		is_simple[current_nt] = is_simple[current_nt] a[2] "\n"
	}
	scheme[++num_schemes] = current_string
}

##(1) 若有产生式P=>a···、或P=>Qa···，则a属于FirstVT（P）；
##(2) 若有a属于FirstVT（Q），且有产生式P=>Q···，则a属于FirstVT(P);
##
func get_firstvt(__ARGV_END__, i, P, tmpset, running)
{
	do {
		running = 0
		for(i = 1; i <= num_schemes; i++) {
			$0 = scheme[i]
			P = $1
			
			if( is_terminator($3) ) {
				tmpset = xset_join(firstvt[P], get_operator($3))
				if(firstvt[P] != tmpset) {
					running = 1
					firstvt[P] = tmpset
				}
			} else {
				if(NF >= 4 && is_terminator($4)) {
					tmpset = xset_join(firstvt[P], get_operator($4))
					if(firstvt[P] != tmpset) {
						running = 1
						firstvt[P] = tmpset
					}
				}
				tmpset = xset_join(firstvt[P], firstvt[$3])
				if(firstvt[P] != tmpset) {
					running = 1
					firstvt[P] = tmpset
				}
			}
		}
	} while(running)
}

##(1) 若有产生式P=>···a或P=>···aQ，则a属于LastVT集。
##(2) 若a属于LastVT（Q），且有产生式P=>···Q，则a属于LastVT集。
func get_lastvt(__ARGV_END__, i, P, tmpset, running)
{
	do {
		running = 0
		for(i = 1; i <= num_schemes; i++) {
			$0 = scheme[i]
			P = $1
			
			if( is_terminator($NF) ) {
				tmpset = xset_join(lastvt[P], get_operator($NF))
				if(lastvt[P] != tmpset) {
					running = 1
					lastvt[P] = tmpset
				}
			} else {
				if(NF >= 4 && is_terminator($(NF-1))) {
					tmpset = xset_join(lastvt[P], get_operator($(NF-1)))
					if(lastvt[P] != tmpset) {
						running = 1
						lastvt[P] = tmpset
					}
				}
				tmpset = xset_join(lastvt[P], lastvt[$NF])
				if(lastvt[P] != tmpset) {
					running = 1
					lastvt[P] = tmpset
				}
			}
		}
	} while(running)
}


func dump_vt(vt, order, arraysize, name, __ARGVEND__, i, n)
{
	print "\n\n" >STDOUT
	for(i = 1; i <= arraysize; i++) {
		printf("%s(%s):  %s\n", name, order[i], vt[order[i]]) >STDOUT
	}
}

func dump_schemes(prefix, array, arraysize, __ARGV_END__, i)
{
	printf("%s %u SCHEMES:\n", prefix, arraysize) >STDERR
	for(i = 1; i <= arraysize; i++) {
		printf("%s\n", array[i]) > STDERR
	}
	printf("\n\n") >STDERR
}

func go_after(t, set, __ARGVEND__, i,n,a)
{
	printf("Put %s '"'LOWER' '{' %s '}'"'\n", t, set) > STDERR
	n = split(set, a)
	for(i = 1; i <= n; i++) {
		precedence[t, a[i]] = "<"
		printf("  %s  '"'<'  %s"'\n", t, a[i]) > STDERR
	}
}

func go_before(set, t, __ARGVEND__, i,n,a,fmt)
{
	printf("Put '"'{' %s '}' 'HIGHER' %s"'\n", set, t) > STDERR
	n = split(set, a)
	for(i = 1; i <= n; i++) {
		precedence[a[i], t] = ">"
		printf("  %s  '"'>'  %s"'\n", a[i], t) > STDERR
	}
}

func output_matrix(matrix, order, array_size, __ARGVEND__, i,j,n,x,y,p,fmt)
{
	if( ENVIRON["GPM_OUTPUT_STYLE"] == "C") {
		oprset_name = ENVIRON["GPM_OPRSET_NAME"]
		oprmx_name  = ENVIRON["GPM_OPRMX_NAME"]
		printf("#ifndef  __OPERATOR_PRECEDENCE_MATRIX_H\n")
		printf("#define  __OPERATOR_PRECEDENCE_MATRIX_H\n\n")

		printf("#ifdef   __OPM_CONST_DATA\n")
		printf("static const char *%s[] = {\n", oprmx_name)
	}

	for(i = 1; i <= array_size; i++) {
		x = order[i]
		for(j = 1; j <= array_size; j++) {
			y = order[j]
			if( (x,y) in matrix )
				p = matrix[x, y]
			else
				p = "X"
			printf("\t\"%s  %s  %s\",\n", x, y, p)
		}
	}
	
	if( ENVIRON["GPM_OUTPUT_STYLE"] == "C") {
		printf("};\n\n\n");

		printf("static const char *%s[] = {\n\t", oprset_name)
		for(i = 1; i <= array_size; i++)
			printf("\"%s\", ", order[i])
		printf("};\n\n");

		printf("static const char *reserved_symbols[] = {\n\t")
		for(i = 1; i <= num_other_symbols; i++) {
			$0 = other_symbols[i]
			printf("\"%s\", ", $2)
		}
		printf("};\n");
		printf("#endif\n\n");

		n = 0
		for(i = 1; i <= array_size; i++) {
			x = order[i]
			if( !(x in opr_symbols) ) {
				printf("Cannot map %s\n", x) > "/dev/stderr"
				exit 2
			}
			printf("#define  %-24s %u\n", opr_symbols[x], n)
			n++
		}
		for(i = 1; i <= num_other_symbols; i++) {
			$0 = other_symbols[i]
			printf("#define  %-24s %u\n", $1, n)
			n++
		}
		printf("#define  %-24s %uU\n", "SSID_INVALID", 0xFFFFFFFF)

		printf("\n#endif\n")
	}
}


func check_refs(x, refs, __ARGV_END__, i, j, a, n1)
{
	if( is_terminator(x) )
		return

	for(i = 1; i <= num_schemes; i++) {
		n1 = split(scheme[i], a)
		if(a[1] != x)
			continue
		for(j = 3; j <= n1; j++) {
			if( ! (a[j] in refs) ) {
				refs[a[j]] = 1
				check_refs(a[j], refs)
			}
		}
	}
}

func remove_unreference(start_symbol, __ARGV_END__, i, count, refs, tmp)
{
	count = 0
	refs[start_symbol] = 1
	check_refs(start_symbol, refs)
	for(i = 1; i <= num_schemes; i++) {
		$0 = scheme[i]
		if(refs[$1] == 0)
			printf("Remove %s\n", $1) >STDERR
		else
			tmp[++count] = scheme[i]
	}
	delete scheme
	for(i = 1; i <= count; i++)
		scheme[i] = tmp[i]
	num_schemes = count
}

FILENAME == ARGV[1] {
	if( ! $1 || $1 ~ /%/)
		next

	if( is_non_terminator($1) ) {
		current_nt = $1
		if( $2 != ":" ) {
			print "Syntax error #1: " $0 > STDERR
			g_status = 2
			exit g_status
		}
		if( !(current_nt in is_simple) )
			is_simple[current_nt] = ""
		start_field = 3
	} else if($1 == "|") {
		start_field = 2
	} else if($1 == ";") {
		current_nt = ""
		next
	} else {
		print "Syntax error: #2" $0 > STDERR
		g_status = 2
		exit g_status
	}

	current_string = current_nt " : "

	for(loop_variable = start_field; loop_variable <= NF; loop_variable++) {
		if( $loop_variable != "|" ) {
			current_string = current_string " " $loop_variable
		} else {
			get_schemes()
			current_string = current_nt " : "
		}
	}

	if(current_string != current_nt " : ")
		get_schemes()
}


FILENAME == ARGV[2] && $2 {
	printf("FILE2: %s\n", FILENAME) >STDERR
	opr_symbols[$2] = $1
	if($2 == "#") {
		flag_on = 1
		next
	}
	if(flag_on)
		other_symbols[++num_other_symbols] = $0
}

END {
	if( g_status != 0 ) {
		print "status: " g_status > STDERR
		exit g_status
	}

	dump_schemes("A", scheme, num_schemes);

	for(x in is_simple)
		if( ! is_simple[x] )
			delete is_simple[x]

	printf("Simple SCHEMES:\n") >STDERR
	count = 0
	for(i in is_simple) {
		++count
		printf("%03u: %s: %s\n", count, i, is_simple[i]) > STDERR
	}
	printf("\n\n\n") >STDERR

################################
#   Subtitute simple schemes   #
################################
	count = 0
	for(i = 1; i <= num_schemes; i++) {
		$0 = scheme[i]
		replace_flag = 0
		if($1 in is_simple)
			continue
		for(j = 3; j <= NF; j++) {
			for(x in is_simple) {
				if(x == $j) {
					printf("Going to expand %s\n", $0) >STDERR
					n1 = split(is_simple[x], a, "\n")
					for(k = 1; k <= n1; k++) {
						if(!a[k])
							continue
						$j = a[k]
						tmp_scheme[++count] = $0
						printf("****: %s\n", $0) >STDERR
						replace_flag = 1
					}
				}
			}
		}
		if( ! replace_flag )
			tmp_scheme[++count] = $0
	}

	delete scheme
	num_schemes = count
	dump_schemes("B", tmp_scheme, num_schemes);

	count = 0
	for(i = 1; i <= num_schemes; i++) {
		$0 = tmp_scheme[i]
		printf("Checking %s\n", $0) >STDERR
		ignore = 0
		for(j = 3; j <= NF; j++) {
			if( (is_terminator($j) && j < NF && is_terminator($(j+1))) || $j ~ /'"'[a-z]+'"'/ ) {
				printf("Ignore %s\n", $0) >STDERR
				ignore = 1
				break
			}
		}
		if( ! ignore )
			scheme[++count] = tmp_scheme[i]
	}

	delete tmp_scheme
	num_schemes = count

	dump_schemes("C", scheme, num_schemes);

	remove_unreference("start")
	dump_schemes("D", scheme, num_schemes);

	for(i = 1; i <= num_schemes; i++) {
		$0 = scheme[i]

		P = $1
		if( ! (P in NT) ) {
			NT[P] = 1
			++num_NT
			NT_ORDER[num_NT] = P
		}

		for(j = 3; j <= NF; j++) {
			opr = get_operator($j)
			if( opr && ! (opr in VT) ) {
				VT[opr] = 1
				++num_VT
				VT_ORDER[num_VT] = opr
			}
		}
	}
##	exit

	printf("Getting FirstVT...\n") >STDERR

	get_firstvt() 
	get_lastvt()

	dump_vt(firstvt, NT_ORDER, num_NT, "FirstVT")
	dump_vt(lastvt, NT_ORDER, num_NT, "LastVT")


	## Construct the Precedence Matrix
	for(i = 1; i <= num_schemes; i++) {
		$0 = scheme[i]
		for(j = 3; j <= NF - 1; j++) {
			j1 = j + 1
			j2 = j + 2
			printf("\n%s\n", $0) > STDOUT
			opr = get_operator($j)
			opr1 = get_operator($j1)
			opr2 = get_operator($j2)
			if( j2 <= NF && opr && opr2 ) {
				printf("Put %s = %s\n", opr, opr2) > STDERR
				precedence[opr,opr2] = "="
			} 
			if ( opr && opr1 ) {
				printf("Put %s = %s\n", opr, opr1) > STDERR
				precedence[opr,opr1] = "="
			} else if( opr && ! opr1 ) {
				go_after(opr, firstvt[$j1]);
			} else if( ! opr && opr1 ) {
				go_before(lastvt[$j], opr1)
			}
		}
	}

	precedence["?",":"] = "<" # !!Hack
	output_matrix(precedence, VT_ORDER, num_VT)
}' "$1" "$2"
