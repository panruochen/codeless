def compare_argument_against_option(a, o) :
    if a + '=' == o :
        return 2
    if a.find(o) == 0 :
        return 1
    return 0

def collect_options(short_options, long_options, args) :
    new_args = [ ]
    others = ''
    need_param = False
    for a in args :
        if need_param :
            new_args.append(a)
            need_param = False
            continue
        matched = False
        n = 0
        if a[0] == '-' and len(a) > 1 :
            if a[1] == '-' :
                n = 2
            else :
                n = 1
            for o in long_options :
                result = compare_argument_against_option(a[n:], o)
                if result > 0 :
                    new_args.append(a)
                    if result == 2 :
                        need_param = True
                    matched = True
                    break
            if not matched :
                i = 0
                size = len(short_options)
                while i < size :
                    o = short_options[i]
                    if o == a[1] :
                        if i+1 < size and short_options[i+1] == ':' :
                            new_args.append(a)
                            if len(a) == 2 :
                                need_param = True
                            i += 1
                            matched = True
                        elif len(a) == 2 :
                            new_args.append(a)
                            matched = True
                        break
                    i += 1
        if not matched :
            others += ' ' + a
    return new_args, others


