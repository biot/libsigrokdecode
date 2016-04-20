/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "libsigrokdecode-internal.h" /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include "libsigrokdecode.h"
#include <inttypes.h>
#include <string.h>

static PyObject *srd_logic_iter(PyObject *self)
{
	return self;
}

static PyObject *srd_logic_iternext(PyObject *self)
{
	srd_logic *logic;
	PyObject *py_samplenum, *py_samples;
	uint8_t *sample_pos, sample;
	unsigned int max_itercnt;
	int byte_offset, bit_offset, i;

	logic = (srd_logic *)self;
	max_itercnt = logic->inbuflen / logic->di->data_unitsize;
	if (logic->itercnt >= max_itercnt) {
		/* End iteration loop. */
		return NULL;
	}

	sample_pos = logic->inbuf + logic->itercnt * logic->di->data_unitsize;
	if (logic->itercnt) {
		/* Find next different sample. */
		while (!memcmp(logic->prev_sample, sample_pos,
				logic->di->data_unitsize)) {
			logic->itercnt++;
			if (logic->itercnt >= max_itercnt) {
				/* End iteration loop. */
				return NULL;
			}
			sample_pos += logic->di->data_unitsize;
		}
	}
	memcpy(logic->prev_sample, sample_pos, logic->di->data_unitsize);

	/*
	 * Convert the bit-packed sample to an array of bytes, with only 0x01
	 * and 0x00 values, so the PD doesn't need to do any bitshifting.
	 */
	for (i = 0; i < logic->di->dec_num_channels; i++) {
		/* A channelmap value of -1 means "unused optional channel". */
		if (logic->di->dec_channelmap[i] == -1) {
			/* Value of unused channel is 0xff, instead of 0 or 1. */
			logic->di->channel_samples[i] = 0xff;
		} else {
			byte_offset = logic->di->dec_channelmap[i] / 8;
			bit_offset = logic->di->dec_channelmap[i] % 8;
			sample = *(sample_pos + byte_offset) & (1 << bit_offset) ? 1 : 0;
			logic->di->channel_samples[i] = sample;
		}
	}

	/* Prepare the next samplenum/sample list in this iteration. */
	py_samplenum =
	    PyLong_FromUnsignedLongLong(logic->start_samplenum +
					logic->itercnt);
	PyList_SetItem(logic->sample, 0, py_samplenum);
	py_samples = PyBytes_FromStringAndSize((const char *)logic->di->channel_samples,
					       logic->di->dec_num_channels);
	PyList_SetItem(logic->sample, 1, py_samples);
	Py_INCREF(logic->sample);
	logic->itercnt++;

	return logic->sample;
}

/** Create the srd_logic type.
 * @return The new type object.
 * @private
 */
SRD_PRIV PyObject *srd_logic_type_new(void)
{
	PyType_Spec spec;
	PyType_Slot slots[] = {
		{ Py_tp_doc, "sigrokdecode logic sample object" },
		{ Py_tp_iter, (void *)&srd_logic_iter },
		{ Py_tp_iternext, (void *)&srd_logic_iternext },
		{ Py_tp_new, (void *)&PyType_GenericNew },
		{ 0, NULL }
	};
	spec.name = "srd_logic";
	spec.basicsize = sizeof(srd_logic);
	spec.itemsize = 0;
	spec.flags = Py_TPFLAGS_DEFAULT;
	spec.slots = slots;

	return PyType_FromSpec(&spec);
}
