/* fifoparser.h:    parsing functions to read from FIFO to cfg
 *
 * Copyright (c) 2012..2018 Sebastian Parschauer <s.parschauer@gmx.de>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU General Public License Version 3, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <cstring>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

// local includes
#include <fifoparser.h>

#define REVERSE_FST -2  // reverse file read start
#define REVERSE_EOF -3  // reverse file read end


ssize_t parse_dynmem_buf (list<CfgEntry> *cfg, void *argp, char *ibuf,
			  ssize_t *ilen, ssize_t tmp_ilen, i32 pmask,
			  bool reverse, ptr_t code_offs, struct parse_cb *pcb)
{
	ptr_t mem_addr = 0, code_addr = 0, stack_offs = 0, stack_end = 0;
	ulong mem_size = 0;
	char *msg_start = ibuf, *msg_end = ibuf, *pstart = ibuf, *sep_pos = ibuf;
	string lib_name;
	struct post_parse pp;
	char scan_ch;

	*ilen += tmp_ilen;

	// parse messages
next:
	if (reverse) {
		msg_end = strrchr(ibuf, '\n');
		if (msg_end == NULL)
			return tmp_ilen;
		else
			*msg_end = '\0';
		msg_start = strrchr(ibuf, '\n');
		if (msg_start == NULL) {
			// incomplete message, restore message end
			*msg_end = '\n';
			return tmp_ilen;
		} else {
			msg_start++;
		}
	} else {
		msg_end = strchr(ibuf, '\n');
		if (msg_end == NULL)
			return tmp_ilen;
	}
	pstart = msg_start;
	if (sscanf(pstart, "%c", &scan_ch) != 1)
		goto parse_err;
	switch (scan_ch) {
	case 'm':
		if (!pcb || !pcb->mf)
			break;
		pstart++;
		if (sscanf(pstart, SCN_PTR, &mem_addr) != 1)
			goto parse_err;

		if (!(pmask & PARSE_S))
			goto skip_s;
		sep_pos = strchr(pstart, ';');
		if (!sep_pos)
			goto parse_err;
		pstart = sep_pos + 1;
		if (sscanf(pstart, "%c", &scan_ch) != 1 ||
		    scan_ch != 's')
			goto parse_err;
		pstart++;
		if (sscanf(pstart, "%lu", &mem_size) != 1)
			goto parse_err;
skip_s:
		if (!(pmask & PARSE_C))
			goto skip_c;
		sep_pos = strchr(pstart, ';');
		if (!sep_pos)
			goto parse_err;
		pstart = sep_pos + 1;
		if (sscanf(pstart, "%c", &scan_ch) != 1 ||
		    scan_ch != 'c')
			goto parse_err;
		pstart++;
		if (sscanf(pstart, SCN_PTR, &code_addr) != 1)
			goto parse_err;
skip_c:
		if (!(pmask & PARSE_O))
			goto skip_o;
		sep_pos = strchr(pstart, ';');
		if (!sep_pos)
			goto parse_err;
		pstart = sep_pos + 1;
		if (sscanf(pstart, "%c", &scan_ch) != 1 ||
		    scan_ch != 'o')
			goto parse_err;
		pstart++;
		if (sscanf(pstart, SCN_PTR, &stack_offs) != 1)
			goto parse_err;
skip_o:
		pp.ibuf = ibuf;
		pp.ppos = pstart - ibuf;
		pp.msg_end = msg_end;
		pp.argp = argp;

		// call post parsing function
		pcb->mf(cfg, &pp, mem_addr, mem_size,
			code_offs, code_addr, stack_offs);
		break;
	case 'f':
		if (!pcb || !pcb->ff)
			break;
		pstart++;
		if (sscanf(pstart, SCN_PTR, &mem_addr) != 1)
			goto parse_err;

		// call post parsing function
		pcb->ff(cfg, argp, mem_addr);
		break;
	case 'l':
		if (!pcb || !pcb->lf)
			break;
		pstart++;
		if (*pstart != ';')
			goto parse_err;
		pstart++;
		lib_name = string(pstart, msg_end - pstart);

		// call post parsing function
		pcb->lf(argp, &lib_name);
		break;
	case 'S':
		if (!pcb || !pcb->sf)
			break;
		pstart++;
		if (sscanf(pstart, SCN_PTR, &stack_end) != 1)
			goto parse_err;
		if (!stack_end)
			break;

		// call post parsing function
		pcb->sf(cfg, argp, stack_end);
		break;
	}

	// prepare for next msg parsing
	tmp_ilen = msg_end - msg_start + 1;
	if (reverse) {
		// zero parsed message
		memset(msg_start, 0, tmp_ilen);
	} else {
		// move rest to the front
		memmove(ibuf, ibuf + tmp_ilen, *ilen - tmp_ilen);
		// zero what's behind the rest
		memset(ibuf + *ilen - tmp_ilen, 0, tmp_ilen);
	}
	*ilen -= tmp_ilen;

	goto next;

parse_err:
	ugerr << "parse error at ppos: " << pstart - ibuf << endl;
	cerr << ibuf;
	memset(ibuf, 0, tmp_ilen);
	*ilen = 0;
	return 0;
}

ssize_t read_dynmem_buf (list<CfgEntry> *cfg, void *argp, i32 ifd, i32 pmask,
			 bool reverse, ptr_t code_offs, struct parse_cb *pcb)
{
	static ssize_t ilen = 0;
	ssize_t tmp_ilen;
	static char ibuf[PIPE_BUF] = { 0 };
	size_t max_read = sizeof(ibuf) - 1 - ilen;   // always '\0' at end
	char *istart = ibuf + ilen;
	char scan_ch;
	ptr_t stack_end = 0;
	static off_t fd_offs = REVERSE_FST;

	if (reverse) {
		if (fd_offs == REVERSE_FST) {
			// read stack end from file first
			tmp_ilen = read(ifd, ibuf, max_read);
			if (tmp_ilen <= 0) {
				tmp_ilen = 0;
				if (argp)
					return -1;
			}
			if (sscanf(ibuf, "%c", &scan_ch) != 1)
				goto parse_err;
			if (scan_ch == 'S') {
				if (sscanf(ibuf + 1, SCN_PTR, &stack_end) != 1)
					goto parse_err;
			}
			if (pcb && pcb->sf && stack_end) {
				// call post parsing function
				pcb->sf(cfg, argp, stack_end);
			}
			// clear buffer again
			memset(ibuf, 0, tmp_ilen);
			// set file pointer to file end
			fd_offs = lseek(ifd, 0, SEEK_END);
		}
		if (fd_offs <= 0)
			return -1;
		// set file offset
		if ((unsigned) fd_offs > max_read) {
			fd_offs = lseek(ifd, fd_offs - max_read, SEEK_SET);
			if (fd_offs < 0)
				return -1;
		} else {
			max_read = (unsigned) fd_offs;
			fd_offs = lseek(ifd, 0, SEEK_SET);
			if (fd_offs < 0)
				return -1;
			fd_offs = REVERSE_EOF;
		}
		// move rest to the end
		if (ilen)
			memmove(ibuf + max_read, ibuf, ilen);

		// read from FIFO and concat. incomplete msgs
		tmp_ilen = read(ifd, ibuf, max_read);  // always '\0' at end
	} else {
		// read from FIFO and concat. incomplete msgs
		tmp_ilen = read(ifd, istart, max_read);  // always '\0' at end
	}
	if (tmp_ilen > 0)
		tmp_ilen = parse_dynmem_buf(cfg, argp, ibuf, &ilen, tmp_ilen,
			pmask, reverse, code_offs, pcb);
	else
		return -1;

	return tmp_ilen;
parse_err:
	ugerr << "parse error at ppos: 0" << endl;
	cerr << ibuf;
	memset(ibuf, 0, sizeof(ibuf));
	ilen = 0;
	return 0;
}
