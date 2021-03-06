/* radare - LGPL - Copyright 2009-2013 - pancake */

// > pxa
#define append(x,y) { strcat(x,y);x += strlen(y); }
static void annotated_hexdump(RCore *core, const char *str, int len) {
	const int usecolor = r_config_get_i (core->config, "scr.color");
	const int COLS = 16;
	const ut8 *buf = core->block;
	ut64 addr = core->offset;
	char *ebytes, *echars;
	ut64 fend = UT64_MAX;
	char *comment;
	int rows = len/COLS;
	char out[1024];
	char *note[COLS];
	int lnote[COLS];
	char bytes[1024];
	char chars[1024];
	int i, j, low, max, marks, tmarks, setcolor, hascolor;
	ut8 ch;
	const char *colors[8] = {
		Color_WHITE, Color_GREEN, Color_YELLOW, Color_RED,
		Color_CYAN, Color_MAGENTA, Color_GRAY, Color_BLUE
	};
	int col = core->print->col;

	if (usecolor) r_cons_strcat (Color_GREEN);
	r_cons_strcat ("- offset -   0 1  2 3  4 5  6 7  8 9  A B  C D  E F  0123456789ABCDEF\n");
	if (usecolor) r_cons_strcat (Color_RESET);
	hascolor = 0;
	tmarks = marks = 0;
	for (i=0; i<rows; i++) {
		bytes[0] = 0;
		ebytes = bytes;
		chars[0] = 0;
		echars = chars;
		hascolor = 0;
		for (j=0; j<COLS; j++) {
			note[j] = NULL;
			lnote[j] = 0;
			// collect comments
			comment = r_meta_get_string (core->anal->meta, R_META_TYPE_COMMENT, addr+j);
			if (comment) {
				comment = r_str_prefix_all (comment, "  ; ");
				r_cons_strcat (comment);
				free (comment);
			}
			// collect flags
			RFlagItem *f = r_flag_get_i (core->flags, addr+j);
			setcolor = 1;
			if (f) {
				fend = addr +j+ f->size;
				note[j] = f->name;
				marks++;
				tmarks++;
			} else {
				if (fend==UT64_MAX || fend<=(addr+j))
					setcolor = 0;
				// use old flag if still valid
			}
			if (setcolor && !hascolor) {
				hascolor = 1;
				if (usecolor) {
#if 1
					append (ebytes, colors[tmarks%5]);
#else
					// psycodelia!
					char *color = r_cons_color_random (0);
					append (ebytes, color);
					free (color);
#endif
				} else {
					append (ebytes, Color_INVERT);
				}
			}
			ch = buf[(i*COLS)+j];
			if (core->print->ocur!=-1) {
				low = R_MIN (core->print->cur, core->print->ocur);
				max = R_MAX (core->print->cur, core->print->ocur);
			} else {
				low = max = core->print->cur;
			}
			if (core->print->cur_enabled) {
				int here = (i*COLS)+j;
				if (low==max) {
					if (low == here) {
						append (echars, Color_INVERT);
						append (ebytes, Color_INVERT);
					}
				} else {
					if (here >= low && here <max) {
						append (ebytes, Color_INVERT);
						append (echars, Color_INVERT);
					}
				}
			}
			sprintf (ebytes, "%02x", ch);
			ebytes += strlen (ebytes);
			sprintf (echars, "%c", IS_PRINTABLE (ch)?ch:'.');
			echars++;
			if (core->print->cur_enabled) {
				if (max == ((i*COLS)+j)) {
					append (ebytes, Color_RESET);
					append (echars, Color_RESET);
					hascolor = 0;
				}
			}
			if (j<15&&j%2) append (ebytes, " ");


			if (fend!=UT64_MAX && fend == addr+j+1) {
				if (usecolor) {
					append (ebytes, Color_RESET);
					append (echars, Color_RESET);
				}
				fend = UT64_MAX;
				hascolor = 0;
			}
		}
		// show comments and flags
		if (marks>0) {
			r_cons_strcat ("            ");
			memset (out, ' ', sizeof (out));
			out[sizeof (out)-1] = 0;
			for (j=0; j<COLS; j++) {
				if (note[j]) {
					int off = (j*3);
					off -= (j/2);
					if (j%2) off--;
					memcpy (out+off, "/", 1);
					memcpy (out+off+1, note[j], strlen (note[j]));
				}
				/// XXX overflow
			}
			out[70] = 0;
			r_cons_strcat (out);
			r_cons_newline ();
			marks = 0;
		}
		if (usecolor) r_cons_strcat (Color_GREEN);
		r_cons_printf ("0x%08"PFMT64x, addr);
		if (usecolor) r_cons_strcat (Color_RESET);
		r_cons_strcat ((col==1)?" |":"  ");
		r_cons_strcat (bytes);
		r_cons_strcat (Color_RESET);
		r_cons_strcat ((col==1)?"| ":(col==2)?" |":"  ");
		r_cons_strcat (chars);
		r_cons_strcat (Color_RESET);
		if (col==2) r_cons_strcat ("|");
		r_cons_newline ();
		addr += 16;
	}
}

R_API void r_core_print_examine(RCore *core, const char *str) {
	char cmd[128], *p;
	ut64 addr = core->offset;
	int size = (core->anal->bits/4);
	int count = atoi (str);
	int i, n;
	if (count<1) count = 1;
	// skipsapces
	while (*str>='0' && *str<='9') str++;
#if 0
Size letters are b(byte), h(halfword), w(word), g(giant, 8 bytes).
#endif
	switch (str[1]) {
	case 'b': size = 1; break;
	case 'h': size = 2; break;
	case 'w': size = 4; break;
	case 'g': size = 8; break;
	}
	if ((p=strchr (str, ' ')))
		addr = r_num_math (core->num, p+1);
	switch (*str) {
	case '?':
		eprintf (
"Format is x/[num][format][size]\n"
"Num specifies the number of format elements to display\n"
"Format letters are o(octal), x(hex), d(decimal), u(unsigned decimal),\n"
"  t(binary), f(float), a(address), i(instruction), c(char) and s(string),\n"
"  T(OSType), A(floating point values in hex).\n"
"Size letters are b(byte), h(halfword), w(word), g(giant, 8 bytes).\n"
);
		break;
	case 's':
		snprintf (cmd, sizeof (cmd), "psb %d @ 0x%"PFMT64x, count*size, addr);
		r_core_cmd0 (core, cmd);
		break;
	case 'o':
		snprintf (cmd, sizeof (cmd), "pxo %d @ 0x%"PFMT64x, count*size, addr);
		r_core_cmd0 (core, cmd);
		break;
	case 'f':
	case 'A': // XXX (float in hex wtf)
		n = 3;
		snprintf (cmd, sizeof (cmd), "pxo %d @ 0x%"PFMT64x,
			count*size, addr);
		strcpy (cmd, "pf ");
		for (i=0;i<count && n<sizeof (cmd);i++)
			cmd[n++] = 'f';
		cmd[n] = 0;
		r_core_cmd0 (core, cmd);
		break;
	case 'a':
	case 'd':
		snprintf (cmd, sizeof (cmd), "pxw %d @ 0x%"PFMT64x, count*size, addr);
		r_core_cmd0 (core, cmd);
		break;
	case 'i':
		snprintf (cmd, sizeof (cmd), "pid %d @ 0x%"PFMT64x, count, addr);
		r_core_cmd0 (core, cmd);
		break;
	}
}

static int printzoomcallback(void *user, int mode, ut64 addr, ut8 *bufz, ut64 size) {
	RCore *core = (RCore *) user;
	int j, ret = 0;
	RListIter *iter;
	RFlagItem *flag;

	switch (mode) {
	case 'p':
		for (j=0; j<size; j++)
			if (IS_PRINTABLE (bufz[j]))
				ret++;
		break;
	case 'f':
		r_list_foreach (core->flags->flags, iter, flag)
			if (flag->offset <= addr  && addr < flag->offset+flag->size)
				ret++;
		break;
	case 's':
		j = r_flag_space_get (core->flags, "strings");
		r_list_foreach (core->flags->flags, iter, flag) {
			if (flag->space == j && ((addr <= flag->offset
					&& flag->offset < addr+size)
					|| (addr <= flag->offset+flag->size
					&& flag->offset+flag->size < addr+size)))
				ret++;
		}
		break;
	case '0': // 0xFF
		for (j=0; j<size; j++)
			if (bufz[j] == 0)
				ret++;
		break;
	case 'F': // 0xFF
		for (j=0; j<size; j++)
			if (bufz[j] == 0xff)
				ret++;
		break;
	case 'e': // entropy
		ret = (ut8) (r_hash_entropy_fraction (bufz, size)*255);
		break;
	case 'h': // head
	default:
		ret = *bufz;
	}
	return ret;
}

R_API void r_core_print_cmp(RCore *core, ut64 from, ut64 to) {
	long int delta = 0;
	int col = core->cons->columns>123;
	ut8 *b = malloc (core->blocksize);
	ut64 addr = core->offset;
	memset (b, 0xff, core->blocksize);
	delta = addr - from;
	r_core_read_at (core, to+delta, b, core->blocksize);
	r_print_hexdiff (core->print, core->offset, core->block,
		to+delta, b, core->blocksize, col);
	free (b);
}

static int pdi(RCore *core, int l, int len, int ilen) {
	int show_offset = r_config_get_i (core->config, "asm.offset");
	int show_bytes = r_config_get_i (core->config, "asm.bytes");
	int decode = r_config_get_i (core->config, "asm.decode");
	int esil = r_config_get_i (core->config, "asm.esil");
	const ut8 *buf = core->block;
	int i, j, ret, err = 0;
	RAsmOp asmop;
	if (l==0) l = len;
	for (i=j=0; j<len && j<l && i<ilen; i+=ret, j++) {
		r_asm_set_pc (core->assembler, core->offset+i);
		ret = r_asm_disassemble (core->assembler, &asmop, buf+i,
			core->blocksize-i);
		if (show_offset)
			r_cons_printf ("0x%08"PFMT64x"  ", core->offset+i);
		if (ret<1) {
			ret = err = 1;
			if (show_bytes)
				r_cons_printf ("%14s%02x  ", "", buf[i]);
			r_cons_printf ("%s\n", "???");
		} else {
			if (show_bytes)
				r_cons_printf ("%16s  ", asmop.buf_hex);
			if (decode || esil) {
				RAnalOp analop = {0};
				char *tmpopstr, *opstr;
				r_anal_op (core->anal, &analop, core->offset+i,
					buf+i, core->blocksize-i);
				tmpopstr = r_anal_op_to_string (core->anal, &analop);
				if (decode) {
					opstr = (tmpopstr)? tmpopstr: strdup (asmop.buf_asm);
				} else if (esil) {
					opstr = strdup (analop.esil);
				} else opstr = strdup (asmop.buf_asm);
				r_cons_printf ("%s\n", opstr);
				free (opstr);
			} else r_cons_printf ("%s\n", asmop.buf_asm);
		}
	}
	return err;
}

static int cmd_print(void *data, const char *input) {
	RCore *core = (RCore *)data;
	int mode, w, p, i, l, len, total[10];
	ut64 off, from, to, at, ate, piece;
	ut32 tbs = core->blocksize;
	ut8 *ptr = core->block;
	RCoreAnalStats *as;
	ut64 n;

	l = len = core->blocksize;
	if (input[0] && input[1]) {
		const char *p = strchr (input, ' ');
		if (p) {
			l = (int) r_num_math (core->num, p+1);
			/* except disasm and memoryfmt (pd, pm) */
			if (input[0] != 'd' && input[0] != 'm' && input[0]!='a') {
				if (l>0) len = l;
				if (l>tbs) {
					if (!r_core_block_size (core, l)) {
						eprintf ("This block size is too big. Did you mean 'p%c @ %s' instead?\n",
							 *input, input+2);
						return R_FALSE;
					}
					l = core->blocksize;
				} else {
					l = len;
				}
			}
		}// else l = 0;
	} else l = len;

	if (len > core->blocksize)
		len = core->blocksize;

	if (input[0] != 'd' && input[0] != 'm' && input[0]!='a') {
		n = core->blocksize_max;
		i = (int)n;
		if (i != n) i = 0;
		if (i && l > i) {
			eprintf ("This block size is too big (%d<%d). Did you mean 'p%c @ %s' instead?\n",
					i, l, *input, input+2);
			return R_FALSE;
		}
	}

	if (input[0] && input[0]!='Z' && input[1] == 'f') {
		RAnalFunction *f = r_anal_fcn_find (core->anal, core->offset,
				R_ANAL_FCN_TYPE_FCN|R_ANAL_FCN_TYPE_SYM);
		if (f) len = f->size;
		else eprintf ("Cannot find function at 0x%08"PFMT64x"\n", core->offset);
	}
	ptr = core->block;
	core->num->value = len;
	if (len>core->blocksize)
		len = core->blocksize;
	switch (*input) {
	case 'w':
		if (input[1]=='n') {
			int i, n = r_num_rand (10);
			ut64 num, base = r_num_get (core->num, "entry0");
			if (!base) base = 0x8048000;
			eprintf ("[+] Analyzing code starting at 0x%08"PFMT64x"...\n", base);
			r_sys_sleep (3);
			eprintf ("[+] Looking for vulnerabilities...\n");
			r_sys_sleep (3);
			eprintf ("[+] Found %d bugs...\n", n);
			for (i=0; i<n; i++) {
				eprintf ("[+] Deeply analyzing bug %d at 0x%08"PFMT64x"...\n",
					i, base+r_num_rand (0xffff));
				r_sys_sleep (1);
			}
			eprintf ("[+] Finding ROP gadgets...\n");
			n = r_num_rand (0x20);
			num = base;
			for (i=0; i<n; i++) {
				num += r_num_rand (0xfff);
				eprintf (" * 0x%08"PFMT64x" %d : %02x %02x ..\n",
					num, r_num_rand (10),
					r_num_rand (0xff), r_num_rand (0xff));
				r_sys_sleep (r_num_rand (2));
			}
			eprintf ("[+] Cooking the shellcode...\n");
			r_sys_sleep (4);
			eprintf ("[+] Launching the exploit...\n");
			r_sys_sleep (1);
			r_sys_cmd ("sh");
		} else {
			char *cwd = r_sys_getdir ();
			if (cwd) {
				eprintf ("%s\n", cwd);
				free (cwd);
			}
		}
		break;
	case 'v':
		mode = input[1];
		w = len? len: core->print->cols * 4;
		if (mode == 'j') r_cons_strcat ("{");
		off = core->offset;
		for (i=0; i<10; i++) total[i] = 0;
		r_core_get_boundaries (core, "file", &from, &to);
		piece = (to-from) / w;
		if (piece<1) piece = 1;
		as = r_core_anal_get_stats (core, from, to, piece);
		//eprintf ("RANGE = %llx %llx\n", from, to);
		switch (mode) {
		case '?':
			r_cons_printf ("Usage: p%%[jh] [pieces]\n");
			r_cons_printf (" pv   show ascii-art bar of metadata in file boundaries\n");
			r_cons_printf (" pvj  show json format\n");
			r_cons_printf (" pvh  show histogram analysis of metadata per block\n");
			return 0;
		case 'j':
			r_cons_printf (
				"\"from\":%"PFMT64d","
				"\"to\":%"PFMT64d","
				"\"blocksize\":%d,"
				"\"blocks\":[", from, to, piece);
			break;
		case 'h':
			r_cons_printf (".-------------.---------------------------------.\n");
			r_cons_printf ("|   offset    | flags funcs cmts imps syms str  |\n");
			r_cons_printf ("|-------------)---------------------------------|\n");
			break;
		default:
			r_cons_printf ("0x%"PFMT64x" [", from);
		}

		len = 0;
		for (i=0; i<w; i++) {
			at = from + (piece*i);
			ate = at + piece;
			p = (at-from)/piece;
			switch (mode) {
			case 'j':
				r_cons_printf ("%s{",len?",":"");
				if ((as->block[p].flags) 
						|| (as->block[p].functions)
						|| (as->block[p].comments)
						|| (as->block[p].imports)
						|| (as->block[p].symbols)
						|| (as->block[p].strings))
					r_cons_printf ("\"offset\":%"PFMT64d",", at), l++;
				// TODO: simplify with macro
				l = 0;
				if (as->block[p].flags) r_cons_printf ("%s\"flags\":%d", l?",":"", as->block[p].flags), l++;
				if (as->block[p].functions) r_cons_printf ("%s\"functions\":%d", l?",":"", as->block[p].functions), l++;
				if (as->block[p].comments) r_cons_printf ("%s\"comments\":%d", l?",":"", as->block[p].comments), l++;
				if (as->block[p].imports) r_cons_printf ("%s\"imports\":%d", l?",":"", as->block[p].imports), l++;
				if (as->block[p].symbols) r_cons_printf ("%s\"symbols\":%d", l?",":"", as->block[p].symbols), l++;
				if (as->block[p].strings) r_cons_printf ("%s\"strings\":%d", l?",":"", as->block[p].strings), l++;
				r_cons_strcat ("}");
				len++;
				break;
			case 'h':
				total[0] += as->block[p].flags;
				total[1] += as->block[p].functions;
				total[2] += as->block[p].comments;
				total[3] += as->block[p].imports;
				total[4] += as->block[p].symbols;
				total[5] += as->block[p].strings;
				if ((as->block[p].flags) 
						|| (as->block[p].functions)
						|| (as->block[p].comments)
						|| (as->block[p].imports)
						|| (as->block[p].symbols)
						|| (as->block[p].strings))
					r_cons_printf ("| 0x%09"PFMT64x" | %4d %4d %4d %4d %4d %4d   |\n", at,
							as->block[p].flags,
							as->block[p].functions,
							as->block[p].comments,
							as->block[p].imports,
							as->block[p].symbols,
							as->block[p].strings);
				break;
			default:
				if (off>=at && off<ate) {
					r_cons_memcat ("^", 1);
				} else {
					if (as->block[p].strings>0)
						r_cons_memcat ("z", 1);
					else if (as->block[p].imports>0)
						r_cons_memcat ("i", 1);
					else if (as->block[p].symbols>0)
						r_cons_memcat ("s", 1);
					else if (as->block[p].functions>0)
						r_cons_memcat ("F", 1);
					else if (as->block[p].flags>0)
						r_cons_memcat ("f", 1);
					else if (as->block[p].comments>0)
						r_cons_memcat ("c", 1);
					else r_cons_memcat (".", 1);
				}
			break;
			}
		}
		switch (mode) {
			case 'j':
				r_cons_strcat ("]}\n");
				break;
			case 'h':
				//r_cons_printf ("  total    | flags funcs cmts imps syms str  |\n");
				r_cons_printf ("|-------------)---------------------------------|\n");
				r_cons_printf ("|    total    | %4d %4d %4d %4d %4d %4d   |\n",
					total[0], total[1], total[2], total[3], total[4], total[5]);
				r_cons_printf ("`-------------'---------------------------------'\n");
				break;
			default:
				r_cons_printf ("] 0x%"PFMT64x"\n", to);
		}
		r_core_anal_stats_free (as);
		break;
	case '=':
		switch (input[1]) {
		case '?': // entropy
			eprintf ("Usage: p=[bep?]\n");
			eprintf (" p=   print bytes of current block in bars\n");
			eprintf (" p=b  same as above\n");
			eprintf (" p=e  same as above but with entropy\n");
			eprintf (" p=p  print number of printable bytes for each filesize/blocksize\n");
			break;
		case 'e': // entropy
			{
			ut8 *p;
			int psz, i = 0;
			int fsz = core->file?core->file->size:0;
			psz = fsz / core->blocksize;
			ptr = malloc (core->blocksize);
			eprintf ("offset = num * %d\n", psz);
			p = malloc (psz);
			for (i=0; i<core->blocksize; i++) {
				r_core_read_at (core, i*psz, p, psz);
				ptr[i] = (ut8) (256 * r_hash_entropy_fraction (p, psz));
			}
			free (p);
			}
			break;
		case 'p': // printable chars
			{
			ut8 *p;
			int psz, i = 0, j, k;
			int fsz = core->file?core->file->size:0;
			psz = fsz/core->blocksize;
			ptr = malloc (core->blocksize);
			p = malloc (psz);
			for (i=0; i<core->blocksize; i++) {
				r_core_read_at (core, i*psz, p, psz);
				for (j=k=0; j<psz; j++) {
					if (IS_PRINTABLE (p[j]))
						k++;
				}
				ptr[i] = k;
			}
			free (p);
			}
			break;
		}
		r_print_fill (core->print, ptr, core->blocksize);
		if (ptr != core->block) {
			free (ptr);
#if 0
			int bsize = 512;
			/* TODO: Reimplement using API */
			char *out = r_sys_cmd_strf ("rahash2 -a entropy -b %d '%s'", bsize, core->file->filename);
			if (out) {
				r_cons_strcat (out);
				free (out);
			}
#endif
		}
		break;
	case 'a':
		if (input[1]=='d') {
			RAsmCode *c;
			r_asm_set_pc (core->assembler, core->offset);
			c = r_asm_mdisassemble_hexstr (core->assembler, input+2);
			if (c) {
				r_cons_puts (c->buf_asm);
				r_asm_code_free (c);
			} else eprintf ("Invalid hexstr\n");
		} else {
			RAsmCode *acode;
			r_asm_set_pc (core->assembler, core->offset);
			acode = r_asm_massemble (core->assembler, input+1);
			if (acode && *acode->buf_hex) {
				r_cons_printf ("%s\n", acode->buf_hex);
				r_asm_code_free (acode);
			}
		}
		break;
	case 'b': {
		ut32 n;
		int i, c;
		char buf[32];
#define P(x) (IS_PRINTABLE(x)?x:'.')
#define SPLIT_BITS(x) memmove (x+5, x+4, 5); x[4]=0
		for (i=c=0; i<len; i++,c++) {
			if (c==0) r_print_offset (core->print, core->offset+i, 0, 0);
			r_str_bits (buf, core->block+i, 8, NULL);
			SPLIT_BITS (buf);
			r_cons_printf ("%s.%s  ", buf, buf+5);
			if (c==3) {
				const ut8 *b = core->block + i-3;
				#define K(x) (b[3-x]<<(8*x))
				n = K(0) | K(1) | K(2) | K(3);
				r_cons_printf ("0x%08x  %c%c%c%c\n",
					n, P(b[0]), P(b[1]), P(b[2]), P(b[3]));
				c = -1;
			}
		}
		}
		break;
	case 'B': {
		const int size = len*8;
		char *buf = malloc (size+1);
		if (buf) {
			r_str_bits (buf, core->block, size, NULL);
			r_cons_printf ("%s\n", buf);
			free (buf);
		} else eprintf ("ERROR: Cannot malloc %d bytes\n", size);
		}
		break;
	case 'I': 
		r_core_print_disasm_instructions (core, len, l);
		break;
	case 'i': 
		switch (input[1]) {
		case '?':
			r_cons_printf ("Usage: pi[df] [num]\n");
			break;
		case 'd':
			pdi (core, l, len, core->blocksize);
			break;
		case 'f':
			{
			RAnalFunction *f = r_anal_fcn_find (core->anal, core->offset,
					R_ANAL_FCN_TYPE_FCN|R_ANAL_FCN_TYPE_SYM);
			if (f) {
				r_core_print_disasm_instructions (core, f->size, l);
			} else {
				r_core_print_disasm_instructions (core,
					core->blocksize, l);
			}
			}
			break;
		default:
			r_core_print_disasm_instructions (core,
				core->blocksize, l);
			break;
		}
		return 0;
	case 'D':
	case 'd':
		switch (input[1]) {
		case 'i': 
			pdi (core, l, len, (*input=='D')? len: core->blocksize);
			return 0;
		case 'a':
			{
				RAsmOp asmop;
				int ret, err = 0;
				ut8 *buf = core->block;
				if (l<1) l = len;
				if (l>=core->blocksize) {
					buf = malloc (l+1);
					r_core_read_at (core, core->offset, buf, l);
				}
				for (i=0; i<l; i++ ) {
					ret = r_asm_disassemble (core->assembler, &asmop,
						buf+i, l-i);
					if (ret<1) {
						ret = err = 1;
						//r_cons_printf ("???\n");
						r_cons_printf ("0x%08"PFMT64x" ???\n", core->offset+i);
					} else r_cons_printf ("0x%08"PFMT64x" %16s  %s\n",
						core->offset+i, asmop.buf_hex, asmop.buf_asm);
				}
				if (buf != core->block)
					free (buf);
				return R_TRUE;
			}
			break;
		case 'r': // pdr
			{
			RAnalFunction *f = r_anal_fcn_find (core->anal, core->offset,
					R_ANAL_FCN_TYPE_FCN|R_ANAL_FCN_TYPE_SYM);
			if (f) {
				RListIter *iter;
				RAnalBlock *b;
				// XXX: hack must be reviewed/fixed in code analysis
				if (r_list_length (f->bbs) == 1) {
					b = r_list_get_top (f->bbs);
					if (b->size > f->size) b->size = f->size;
				}
				// TODO: sort by addr
				r_list_foreach (f->bbs, iter, b) {
					r_core_cmdf (core, "pD %"PFMT64d" @0x%"PFMT64x, b->size, b->addr);
					if (b->jump != UT64_MAX)
						r_cons_printf ("-[true]-> 0x%08"PFMT64x"\n", b->jump);
					if (b->fail != UT64_MAX)
						r_cons_printf ("-[false]-> 0x%08"PFMT64x"\n", b->fail);
					r_cons_printf ("--\n");
				}
			} else eprintf ("Cannot find function at 0x%08"PFMT64x"\n", core->offset);
			return R_TRUE;
			}
			break;
		case 'b': {
			RAnalBlock *b = r_anal_bb_from_offset (core->anal, core->offset);
			if (b) {
				ut8 *block = malloc (b->size+1);
				if (block) {
					r_core_read_at (core, b->addr, block, b->size);
					core->num->value = r_core_print_disasm (
						core->print, core, b->addr, block,
						b->size, 9999, 0, 2);
					free (block);
					return 0;
				}
			} else eprintf ("Cannot find function at 0x%08"PFMT64x"\n", core->offset);
			} break;
			break;
		case 'f': {
			RAnalFunction *f = r_anal_fcn_find (core->anal, core->offset,
					R_ANAL_FCN_TYPE_FCN|R_ANAL_FCN_TYPE_SYM);
			if (f) {
				ut8 *block = malloc (f->size+1);
				if (block) {
					r_core_read_at (core, f->addr, block, f->size);
					core->num->value = r_core_print_disasm (
						core->print, core, f->addr, block,
						f->size, 9999, 0, 2);
					free (block);
					return 0;
				}
			} else eprintf ("Cannot find function at 0x%08"PFMT64x"\n", core->offset);
			} break;
		case 'l':
			{
			RAsmOp asmop;
			int j, ret;
			const ut8 *buf = core->block;
			if (l==0) l= len;
			for (i=j=0; i<core->blocksize && j<l; i+=ret,j++ ) {
				ret = r_asm_disassemble (core->assembler, &asmop, buf+i, len-i);
				printf ("%d\n", ret);
				if (ret<1) ret = 1;
			}
			return 0;
			}
			break;
		case 'j':
			r_core_print_disasm_json (core, core->offset,
				core->block, core->blocksize);
			return 0;
		case '?':
			eprintf ("Usage: pd[f|i|l] [len] @ [addr]\n"
			//TODO: eprintf ("  pdr  : disassemble resume\n");
			"  pda  : disassemble all possible opcodes (byte per byte)\n"
			"  pdj  : disassemble to json\n"
			"  pdb  : disassemble basic block\n"
			"  pdr  : recursive disassemble across the function graph\n"
			"  pdf  : disassemble function\n"
			"  pdi  : like 'pi', with offset and bytes\n"
			"  pdl  : show instruction sizes\n");
			return 0;
		}
		//if (core->visual)
		//	l = core->cons->rows-core->cons->lines;
		if (l<0) {
			RList *bwdhits;
			RListIter *iter;
			RCoreAsmHit *hit;
			ut8 *block = malloc (core->blocksize);
			if (block) {
				l = -l;
				bwdhits = r_core_asm_bwdisassemble (core,
					core->offset, l, core->blocksize);
				if (bwdhits) {
					r_list_foreach (bwdhits, iter, hit) {
						r_core_read_at (core, hit->addr,
							block, core->blocksize);
						core->num->value = r_core_print_disasm (core->print,
							core, hit->addr, block, core->blocksize, l, 0, 1);
						r_cons_printf ("------\n");
					}
					r_list_free (bwdhits);
				}
				free (block);
			}
		} else {
			core->num->value = r_core_print_disasm (
				core->print, core, core->offset,
				core->block, len, l, 0, (*input=='D'));
		}
		break;
	case 's':
		switch (input[1]) {
		case '?':
			r_cons_printf ("Usage: ps[zpw] [N]\n"
				" ps  = print string\n"
				" psb = print strings in current block\n"
				" psx = show string with scaped chars\n"
				" psz = print zero terminated string\n"
				" psp = print pascal string\n"
				" psw = print wide string\n");
			break;
		case 'x':
			r_print_string (core->print, core->offset, core->block, len, 0);
			break;
		case 'b':
			{
				char *s = malloc (core->blocksize+1);
				int i, j, hasnl = 0;;
				if (s) {
					memset (s, 0, core->blocksize);
					// TODO: filter more chars?
					for (i=j=0;i<core->blocksize; i++) {
						char ch = (char)core->block[i];
						if (!ch) {
							if (!hasnl) {
								if (*s) r_cons_printf ("%s\n", s);
								j = 0;
								s[0] = 0;
							}
							hasnl = 1;
							continue;
						}
						hasnl = 0;
						if (IS_PRINTABLE (ch))
							s[j++] = ch;
					}
					r_cons_printf ("%s", s); // TODO: missing newline?
					free (s);
				}
			}
			break;
		case 'z':
			{
				char *s = malloc (core->blocksize+1);
				int i, j;
				if (s) {
					memset (s, 0, core->blocksize);
					// TODO: filter more chars?
					for (i=j=0;i<core->blocksize; i++) {
						char ch = (char)core->block[i];
						if (!ch) break;
						if (IS_PRINTABLE (ch))
							s[j++] = ch;
					}
					r_cons_printf ("%s\n", s);
					free (s);
				}
			}
			break;
		case 'p':
			{
			int mylen = core->block[0];
			// TODO: add support for 2-4 byte length pascal strings
			if (mylen < core->blocksize) {
				r_print_string (core->print, core->offset,
					core->block+1, mylen, R_PRINT_STRING_ZEROEND);
				core->num->value = mylen;
			} else core->num->value = 0; // error
			}
			break;
		case 'w':
			r_print_string (core->print, core->offset, core->block, len,
				R_PRINT_STRING_WIDE | R_PRINT_STRING_ZEROEND);
			break;
		case ' ':
			len = r_num_math (core->num, input+2);
			r_print_string (core->print, core->offset, core->block, len, 0);
			break;
		default:
			r_print_string (core->print, core->offset, core->block, len,
				R_PRINT_STRING_ZEROEND);
			break;
		}
		break;
	case 'm':
		if (input[1]=='?') {
			r_cons_printf ("Usage: pm [file|directory]\n"
				" r_magic will use given file/dir as reference\n"
				" output of those magic can contain expressions like:\n"
				"   foo@0x40   # use 'foo' magic file on address 0x40\n"
				"   @0x40      # use current magic file on address 0x40\n"
				"   \\n         # append newline\n"
				" e dir.magic  # defaults to "R_MAGIC_PATH"\n"
				);
		} else r_core_magic (core, input+1, R_TRUE);
		break;
	case 'u':
		r_print_string (core->print, core->offset, core->block, len,
			R_PRINT_STRING_URLENCODE |
			((input[1]=='w')?R_PRINT_STRING_WIDE:0));
		break;
	case 'c':
		r_print_code (core->print, core->offset, core->block, len, input[1]);
		break;
	case 'r':
		r_print_raw (core->print, core->block, len);
		break;
	case 'x':
		{
		int show_offset = r_config_get_i (core->config, "asm.offset");
		if (show_offset) {
			core->print->flags |= R_PRINT_FLAGS_HEADER;
			core->print->flags |= R_PRINT_FLAGS_OFFSET;
		} else {
			core->print->flags &= ~R_PRINT_FLAGS_OFFSET;
			core->print->flags &= ~R_PRINT_FLAGS_HEADER;
		}
		}
		switch (input[1]) {
		case '/':
			r_core_print_examine (core, input+2);
			break;
		case '?':
			eprintf ("Usage: px[afoswqWqQ][f]\n"
				" px     show hexdump\n"
				" px/    same as x/ in gdb (help x)\n"
				" pxa    show annotated hexdump\n"
				" pxf    show hexdump of current function\n"
				" pxo    show octal dump\n"
				" pxq    show hexadecimal quad-words dump (64bit)\n"
				" pxs    show hexadecimal in sparse mode\n"
				" pxQ    same as above, but one per line\n"
				" pxw    show hexadecimal words dump (32bit)\n"
				" pxW    same as above, but one per line\n"
				);
			break;
		case 'a':
			if (len%16)
				len += 16-(len%16);
			annotated_hexdump (core, input+2, len);
			break;
		case 'o':
			r_print_hexdump (core->print, core->offset, core->block, len, 8, 1);
			break;
		case 'w':
			r_print_hexdump (core->print, core->offset, core->block, len, 32, 4);
			break;
		case 'W':
			for (i=0; i<len; i+=4) {
				ut32 *p = (ut32*)core->block+i;
				r_cons_printf ("0x%08"PFMT64x" 0x%08x\n", core->offset+i, *p);
			}
			break;
		case 'q':
			r_print_hexdump (core->print, core->offset, core->block, len, 64, 8);
			break;
		case 'Q':
			for (i=0; i<len; i+=8) {
				ut64 *p = (ut64*)core->block+i;
				r_cons_printf ("0x%08"PFMT64x" 0x%016"PFMT64x"\n",
					core->offset+i, *p);
			}
			break;
		case 's':
			core->print->flags |= R_PRINT_FLAGS_SPARSE;
			r_print_hexdump (core->print, core->offset,
				core->block, len, 16, 1);
			core->print->flags &= (((ut32)-1) & (~R_PRINT_FLAGS_SPARSE));
			break;
		default: {
				 ut64 from = r_config_get_i (core->config, "diff.from");
				 ut64 to = r_config_get_i (core->config, "diff.to");
				 if (from == to && from == 0) {
					 r_print_hexdump (core->print, core->offset,
						core->block, len, 16, 1);
				 } else {
					 r_core_print_cmp (core, from, to);
				 }
			 }
			break;
		}
		break;
	case '6':
		{
		int malen = (core->blocksize*4)+1;
		ut8 *buf = malloc (malen);
		if (!buf) break;
		memset (buf, 0, malen);
		switch (input[1]) {
		case 'd':
			if (r_base64_decode (buf, core->block, len))
				r_cons_printf ("%s\n", buf);
			else eprintf ("r_base64_decode: invalid stream\n");
			break;
		case '?':
			eprintf ("Usage: p6[ed] [len]     base 64 encode/decode\n");
			break;
		case 'e':
		default:
			r_base64_encode (buf, core->block, len); //core->blocksize);
			r_cons_printf ("%s\n", buf);
			break;
		}
		free (buf);
		}
		break;
	case '8':
		r_print_bytes (core->print, core->block, len, "%02x");
		break;
	case 'f':
		if (input[1]=='.') {
			if (input[2]=='\0') {
				RListIter *iter;
				RStrHT *sht = core->print->formats;
				int *i;
				r_list_foreach (sht->ls, iter, i) {
					int idx = ((int)(size_t)i)-1;
					const char *key = r_strpool_get (sht->sp, idx);
					const char *val = r_strht_get (core->print->formats, key);
					r_cons_printf ("pf.%s %s\n", key, val);
				}
			} else
			if (input[2]=='-') {
				if (input[3]) r_strht_del (core->print->formats, input+3);
				else r_strht_clear (core->print->formats);
			} else {
				char *name = strdup (input+2);
				char *space = strchr (name, ' ');
				if (space) {
					*space++ = 0;
					//printf ("SET (%s)(%s)\n", name, space);
					r_strht_set (core->print->formats, name, space);
					return 0;
				} else {
					const char *fmt;
					char *eq, *dot = strchr (name, '.');
					if (dot) {
						// TODO: support multiple levels
						*dot++ = 0;
						eq = strchr (dot, '=');
						if (eq) {
							char *res;
							fmt = r_strht_get (core->print->formats, name);
							// TODO: spaguettti, reuse code below.. and handle atoi() too
							if (fmt) {
								res = strdup (fmt);
								*eq++ = 0;
#if 0
								ut64 v;
								v = r_num_math (NULL, eq);
								r_print_format (core->print, core->offset,
										core->block, core->blocksize, fmt, v, eq);
#endif
								r_str_word_set0 (res);
								for (i = 1; ; i++) {
									const char *k = r_str_word_get0 (res, i);
									if (!k) break;
									if (!strcmp (k, dot)) {
										r_print_format (core->print, core->offset,
												core->block, core->blocksize, fmt, i-1, eq);
										break;
									}
								}
								free (res);
							}
						} else {
							const char *k, *fmt = r_strht_get (core->print->formats, name);
							if (fmt) {
								if (atoi (dot)>0 || *dot=='0') {
									// indexed field access
									r_print_format (core->print, core->offset,
											core->block, core->blocksize, fmt, atoi (dot), NULL);
								} else {
									char *res = strdup (fmt);
									r_str_word_set0 (res);
									for (i = 1; ; i++) {
										k = r_str_word_get0 (res, i);
										if (!k) break;
										if (!strcmp (k, dot)) {
											r_print_format (core->print, core->offset,
												core->block, core->blocksize, fmt, i-1, NULL);
											break;
										}
									}
									free (res);
								}
							} else {
								
							}
						}
					} else {
						const char *fmt = r_strht_get (core->print->formats, name);
						if (fmt) {
							//printf ("GET (%s) = %s\n", name, fmt);
							r_print_format (core->print, core->offset,
								core->block, len, fmt, -1, NULL);
						} else eprintf ("Unknown format (%s)\n", name);
					}
				}
				free (name);
			}
		} else r_print_format (core->print, core->offset,
			core->block, len, input+1, -1, NULL);
		break;
	case 'k':
		{
		char *s = r_print_randomart (core->block, core->blocksize, core->offset);
		r_cons_printf ("%s\n", s);
		free (s);
		}
		break;
	case 'K':
		{
		int w, h;
		RConsCanvas *c;
		w = r_cons_get_size (&h);
		ut64 offset0 = core->offset;
		int cols = (w/20);
		int rows = (h/12);
		int i, j;
		char *s;
		if (rows<1) rows = 1;
		c = r_cons_canvas_new (w, rows*11);
		for (i = 0; i<rows; i++) {
			for (j = 0; j<cols; j++) {
				r_cons_canvas_gotoxy (c, j*20, i*11);
				core->offset += core->blocksize;
				r_core_read_at (core, core->offset, core->block, core->blocksize);
				s = r_print_randomart (core->block, core->blocksize, core->offset);
				r_cons_canvas_write (c, s);
				free (s);
			}
		}
		r_cons_canvas_print (c);
		r_cons_canvas_free (c);
		r_core_read_at (core, offset0, core->block, core->blocksize);
		core->offset = offset0;
		}
		break;
	case 'n': // easter penis
		for (l=0; l<10; l++) {
			printf ("\r8");
			for (len=0; len<l; len++)
				printf ("=");
			printf ("D");
			r_sys_usleep (100000);
			fflush (stdout);
		}
		for (l=0; l<3; l++) {
			printf ("~");
			fflush (stdout);
			r_sys_usleep (100000);
		}
		printf ("\n");
		break;
	case 't':
		switch (input[1]) {
		case ' ':
		case '\0':
			for (l=0; l<len; l+=sizeof (time_t))
				r_print_date_unix (core->print, core->block+l, sizeof (time_t));
			break;
		case 'd':
			for (l=0; l<len; l+=4)
				r_print_date_dos (core->print, core->block+l, 4);
			break;
		case 'n':
			core->print->big_endian = !core->print->big_endian;
			for (l=0; l<len; l+=sizeof (ut64))
				r_print_date_w32 (core->print, core->block+l, sizeof (ut64));
			core->print->big_endian = !core->print->big_endian;
			break;
		case '?':
			r_cons_printf (
			"Usage: pt[dn?]\n"
			" pt      print unix time (32 bit cfg.big_endian)\n"
			" ptd     print dos time (32 bit cfg.big_endian)\n"
			" ptn     print ntfs time (64 bit !cfg.big_endian)\n"
			" pt?     show help message\n");
			break;
		}
		break;
	case 'z':
		if (input[1]=='?') {
			r_cons_printf (
			"Usage: pz [len]\n"
			" print N bytes where each byte represents a block of filesize/N\n"
			"Configuration:\n"
			" zoom.maxsz : max size of block\n"
			" zoom.from  : start address\n"
			" zoom.to    : end address\n"
			" zoom.byte  : specify how to calculate each byte\n"
			"   p : number of printable chars\n"
			"   f : count of flags in block\n"
			"   s : strings in range\n"
			"   0 : number of bytes with value '0'\n"
			"   F : number of bytes with value 0xFF\n"
			"   e : calculate entropy and expand to 0-255 range\n"
			"   h : head (first byte value)\n"
			"WARNING: On big files, use 'zoom.byte=h' or restrict ranges\n");
		} else {
			char *oldzoom = NULL;
			ut64 maxsize = r_config_get_i (core->config, "zoom.maxsz");
			ut64 from, to;
			int oldva = core->io->va;

			from = 0;
			core->io->va = 0;
			to = r_io_size (core->io);
			from = r_config_get_i (core->config, "zoom.from");
			to = r_config_get_i (core->config, "zoom.to");
			if (input[1] && input[1] != ' ') {
				oldzoom = strdup (r_config_get (core->config, "zoom.byte"));
				if (!r_config_set (core->config, "zoom.byte", input+1)) {
					eprintf ("Invalid zoom.byte mode (%s)\n", input+1);
					free (oldzoom);
					return R_FALSE;
				}
			}
			r_print_zoom (core->print, core, printzoomcallback,
				from, to, core->blocksize, (int)maxsize);
			if (oldzoom) {
				r_config_set (core->config, "zoom.byte", oldzoom);
				free (oldzoom);
			}
			if (oldva)
				core->io->va = oldva;
		}
		break;
	default:
		r_cons_printf (
		"Usage: p[=68abcdDfiImrstuxz] [arg|len]\n"
		" p=               show entropy bars of full file\n"
		" p6[de] [len]     base64 decode/encode\n"
		" p8 [len]         8bit hexpair list of bytes\n"
		" pa[d] [hex|asm]  print code from hex or bytes of given data\n"
		" p[bB] [len]      bitstream of N bytes\n"
		" pc[p] [len]      output C (or python) format\n"
		" p[dD][lf] [l]    disassemble N opcodes/bytes (see pd?)\n"
		" pf[?|.nam] [fmt] print formatted data (pf.name, pf.name $<expr>) \n"
		" p[iI][df] [len]  print N instructions/bytes (f=func) (see pi? and pdi)\n"
		" pm [magic]       print libmagic data (pm? for more information)\n"
		" pr [len]         print N raw bytes\n"
		" p[kK] [len]      print key in randomart (K is for mosaic)\n"
		" ps[pwz] [len]    print pascal/wide/zero-terminated strings\n"
		" pt[dn?] [len]    print different timestamps\n"
		" pu[w] [len]      print N url encoded bytes (w=wide)\n"
		" pv[jh] [mode]    bar|json|histogram blocks (mode: e?search.in)\n"
		" p[xX][owq] [len] hexdump of N bytes (o=octal, w=32bit, q=64bit)\n"
		" pz [len]         print zoom view (see pz? for help)\n"
		" pwd              display current working directory\n");
		break;
	}
	if (tbs != core->blocksize)
		r_core_block_size (core, tbs);
	return 0;
}

static int cmd_hexdump(void *data, const char *input) {
	return cmd_print (data, input-1);
}

// TODO : move to r_util? .. depends on r_cons...
R_API void r_print_offset(RPrint *p, ut64 off, int invert, int opt) {
	int show_color = p->flags & R_PRINT_FLAGS_COLOR;
	if (show_color) {
		const char *k = r_cons_singleton ()->pal.offset; // TODO etooslow. must cache
		if (invert)
			r_cons_invert (R_TRUE, R_TRUE);
		if (opt) {
			ut32 s, a;
			a = off & 0xffff;
			s = (off-a)>>4;
			r_cons_printf ("%s%04x:%04x"Color_RESET,
				k, s&0xFFFF, a&0xFFFF);
		} else r_cons_printf ("%s0x%08"PFMT64x""Color_RESET, k, off);
		r_cons_puts (" ");
	} else {
		if (opt) {
			ut32 s, a;
			a = off & 0xffff;
			s = (off-a)>>4;
			r_cons_printf ("%04x:%04x", s&0xFFFF, a&0xFFFF);
		} else {
			r_cons_printf ("0x%08"PFMT64x" ", off);
		}
	}
}
