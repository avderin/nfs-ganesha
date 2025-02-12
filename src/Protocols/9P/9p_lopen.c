// SPDX-License-Identifier: LGPL-3.0-or-later
/*
 * vim:noexpandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2011)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * ---------------------------------------
 */

/**
 * \file    9p_lopen.c
 * \brief   9P version
 *
 * 9p_lopen.c : _9P_interpretor, request LOPEN
 *
 *
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include "nfs_core.h"
#include "log.h"
#include "fsal.h"
#include "9p.h"

int _9p_lopen(struct _9p_request_data *req9p, u32 *plenout, char *preply)
{
	char *cursor = req9p->_9pmsg + _9P_HDR_SIZE + _9P_TYPE_SIZE;
	u16 *msgtag = NULL;
	u32 *fid = NULL;
	u32 *flags = NULL;

	fsal_status_t fsal_status;
	fsal_openflags_t openflags = 0;

	struct _9p_fid *pfid = NULL;

	/* Get data */
	_9p_getptr(cursor, msgtag, u16);
	_9p_getptr(cursor, fid, u32);
	_9p_getptr(cursor, flags, u32);

	LogDebug(COMPONENT_9P, "TLOPEN: tag=%u fid=%u flags=0x%x",
		 (u32) *msgtag, *fid, *flags);

	if (*fid >= _9P_FID_PER_CONN)
		return _9p_rerror(req9p, msgtag, ERANGE, plenout, preply);

	pfid = req9p->pconn->fids[*fid];

	/* Check that it is a valid fid */
	if (pfid == NULL || pfid->pentry == NULL) {
		LogDebug(COMPONENT_9P, "request on invalid fid=%u", *fid);
		return _9p_rerror(req9p, msgtag, EIO, plenout, preply);
	}

	_9p_openflags2FSAL(flags, &openflags);

	pfid->state->state_data.fid.share_access =
		_9p_openflags_to_share_access(flags);

	_9p_init_opctx(pfid, req9p);
	if (pfid->pentry->type == REGULAR_FILE) {
		/** @todo: Maybe other types (FIFO, SOCKET,...) require
		 * to be opened too */
		if (*flags & 0x10)
			openflags |= FSAL_O_TRUNC;

		fsal_status = fsal_reopen2(pfid->pentry, pfid->state,
					   openflags, true);

		if (FSAL_IS_ERROR(fsal_status))
			return _9p_rerror(req9p, msgtag,
					  _9p_tools_errno(fsal_status),
					  plenout, preply);

		atomic_inc_uint32_t(&pfid->opens);

		/* Get a long term reference for every open */
		pfid->ppentry->obj_ops->get_long_term_ref(pfid->ppentry);
	}

	/* Build the reply */
	_9p_setinitptr(cursor, preply, _9P_RLOPEN);
	_9p_setptr(cursor, msgtag, u16);

	_9p_setqid(cursor, pfid->qid);
	_9p_setvalue(cursor, _9P_IOUNIT, u32);

	_9p_setendptr(cursor, preply);
	_9p_checkbound(cursor, preply, plenout);

	LogDebug(COMPONENT_9P,
		 "RLOPEN: tag=%u fid=%u qid=(type=%u,version=%u,path=%llu) iounit=%u",
		 *msgtag, *fid, (u32) pfid->qid.type, pfid->qid.version,
		 (unsigned long long)pfid->qid.path, _9P_IOUNIT);

	return 1;
}
