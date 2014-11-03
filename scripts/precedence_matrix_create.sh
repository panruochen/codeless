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
		echo "$opt";
		export GPM_VERBOSE=$OPTARG;;
	esac
done

shift $((OPTIND-1))

if [ ! -f "$1" ]; then
	echo "No such file: $1" >&2
	exit 1
fi

awk -v VERBOSE=1 -- '
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

func is_non_terminator(c, __ARGVEND__, c1) {
	c1 = substr(c, 1, 1)
	return c1 ~ /[A-Z]/
}

func is_operator(opr, __ARGVEND__, c) {
	c = substr(opr, 1, 1)
	return c == tolower(c)
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
}

{
	split($0, a, ";")
	if( ! a[1] )
		next
	$0 = a[1]
	delete a
	if( ! $1 )
		next

	if( ! is_non_terminator($1) ) {
		print "Invalid OPG: " $0 > STDERR
		g_status = 1
		exit g_status
	}
	if( $2 != "->" ) {
		print "Invalid OPG: " $0 > STDERR
		g_status = 2
		exit g_status
	}
	scheme[++scheme_num] = $0

	A = $1
	##
	## Get FIRSTVT
	if( is_non_terminator($3) ) {
		tmp_first[A] = tmp_first[A] " " $3

		if(NF >= 4) {
			# A -> ...Ba
			if( is_operator($4) )
				firstvt[A] = firstvt[A] " " $4
			else {
				print "Invalid OPG: " $0 > STDERR
				g_status = 3
				exit g_status
			}
		}
	} else if( is_operator($3) ) # A -> a
		firstvt[A] = firstvt[A] " " $3
#	printf("++ FirstVT(%s): %s\n", A, firstvt[A]) >STDERR

	##
	## Get LASTVT
	if( is_non_terminator($NF) ) {
		tmp_last[A] = tmp_last[A] " " $NF
		if( NF >= 4 && is_operator($(NF-1)) )
			lastvt[A] = lastvt[A] " " $(NF-1)
	} else {
#		printf("Got %s\n", $NF) >STDERR
		lastvt[A] = lastvt[A] " " $NF
	}

	if( ! (A in NT) ) {
		NT[A] = 1
		NT_ORDER[length(NT)] = A
	}


	for(i = 3; i <= NF; i++) 
		if( is_operator($i) )
			if( ! ($i in VT) ) {
				VT[$i] = 1
				VT_ORDER[length(VT)] = $i
			}
	
#	for(i = 1; i <= length(VT_ORDER); i++) 
#		printf("%s\n", VT_ORDER[i]) > STDERR
}

func dump_vt(vt, order, arraysize, name, __ARGVEND__, i, n)
{
	print "\n\n" >STDOUT
	for(i = 1; i <= arraysize; i++) {
		printf("%s(%s):  %s\n", name, order[i], vt[order[i]]) >STDOUT
	}
}

func go_after(t, set, __ARGVEND__, i,n,a)
{
	printf("Put %s \"LOWER\" \"{\" %s \"}\"\n", t, set) > STDERR
	n = split(set, a)
	for(i = 1; i <= n; i++) {
		precedence[t, a[i]] = "<"
		printf("Lower: %s \"<\" %s\n", t, a[i]) > STDERR
	}
}

func go_before(set, t, __ARGVEND__, i,n,a,fmt)
{
	fmt = "Put " RED_FONT "%s" RED_FONT "%s\n"
	printf(fmt, "{ ", set, " } > ", t) > STDERR
	n = split(set, a)
	for(i = 1; i <= n; i++) {
		precedence[a[i], t] = ">"
		printf("Higher: %s \">\" %s\n", a[i], t) > STDERR
	}
}

func output_matrix(matrix, order, style, __ARGVEND__, i,j,n,a,b,p,fmt)
{
	n = length(order)
	
	a = 0
	b = 0
	for(i = 1; i <= n; i++) {
		b = length(order[i])
		if(a < b)
			a = b
	}

	if( ! ENVIRON["GPM_OPRSET_NAME"] )
		an_oprset = "g_oprset";
	if( ! ENVIRON["GPM_OPRMX_NAME"] )
		an_oprmx = "g_oprmx";
	
	fmt = sprintf("%%-%ds", a+1)

	printf(fmt,"")
	for(i = 1; i <= n; i++)
		printf(fmt, order[i])
	printf("\n")

	for(i = 1; i <= n; i++) {
		a = VT_ORDER[i]
			printf(fmt, a)
		for(j = 1; j <= n; j++) {
			b = order[j]
			if( (a,b) in matrix )
				p = matrix[a, b]
			else
				p = "X"
			printf(fmt, p)
		}
		printf("\n")
	}
}

END {
	if( g_status != 0 ) {
		print "status: " g_status > STDERR
		exit g_status
	}

	do {
		accomplish = 1
		for(x in firstvt) {
			n = split(tmp_first[x], a)
			for(i = 1; i <= n; i++) {
				if( a[i] != x ) {
					tmpset = xset_join(firstvt[x], firstvt[a[i]]);
					if(tmpset != firstvt[x]) {
						firstvt[x] = tmpset
						accomplish = 0
					}
				}
			}
		}
	} while( ! accomplish ); 

	do {
		accomplish = 1
		for(x in lastvt) {
			n = split(tmp_last[x], a)
			for(i = 1; i <= n; i++) {
				if( a[i] != x ) {
					tmpset = xset_join(lastvt[x], lastvt[a[i]]);
					if(tmpset != lastvt[x]) {
						lastvt[x] = tmpset
						accomplish = 0
					}
				}
			}
		}
	} while( ! accomplish ); 

	dump_vt(firstvt, NT_ORDER, length(NT_ORDER), "FirstVT")
	dump_vt(lastvt, NT_ORDER, length(NT_ORDER), "LastVT")

	## Construct the Precedence Matrix
	for(i = 1; i <= scheme_num; i++) {
		$0 = scheme[i]
		for(j = 3; j <= NF - 1; j++) {
			j1 = j + 1
			j2 = j + 2
			printf("\n%s\n", $0) > STDOUT
			if( j2 <= NF && is_operator($j) && is_operator($j2) ) {
				printf("Put %s = %s\n", $j, $j2) > STDERR
				precedence[$j,$j2] = "="
			} 
			if ( is_operator($j) && is_operator($j1) ) {
				printf("Put %s = %s\n", $j, $j1) > STDERR
				precedence[$j,$j1] = "="
			} else if( is_operator($j) && is_non_terminator($j1) ) {
				go_after($j, firstvt[$j1]);
			} else if( is_non_terminator($j) && is_operator($j1) ) {
				go_before(lastvt[$j], $j1)
			}
		}
	}

	output_matrix(precedence, VT_ORDER)
}' "$1" | {
	if test "$GPM_OUTPUT_STYLE" = "C"; then
		awk -- '{
	if(FNR == 1) {
		oprset_name = ENVIRON["GPM_OPRSET_NAME"];
		oprmx_name  = ENVIRON["GPM_OPRMX_NAME"]
		if( ! oprset_name )
			oprset_name = "g_oprset";
		if( ! oprmx_name )
			oprmx_name = "g_oprmx";
		printf("static const char *%s[%d] = {\n\t", oprset_name, NF)
		for(i = 1; i <= NF; i++)
			printf("\"%s\", ", $i)
		printf("};\n\n")
		printf("static const char %s[%d * %d] = {\n  ", oprmx_name, NF, NF)
	} else {
		for(i = 2; i <= NF; i++)
			printf("%c%s%c, ", 39, $i, 39)
		printf("\n  ")
	}
}
END {
	printf("};")
}
'
	else
		tee
	fi
}

