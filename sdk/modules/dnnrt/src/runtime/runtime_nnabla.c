/****************************************************************************
 * modules/dnnrt/src/runtime/runtime_nnabla.c
 *
 *   Copyright 2018 Sony Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sony Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <dnnrt/runtime.h>

/* header inclusion under $(SDKDIR)/../externals/nnabla-c-runtime/include */
#include "nnablart/runtime.h"

#include <context.h>
#include <runtime_internal.h>
#include "runtime_common.h"

#define WEIGHT (1)

static struct dnn_global_context s_dnn_gctx;

int dnn_initialize(void *reserved)
{
  return reserved == NULL ? RT_RET_NOERROR : -EINVAL;
}

int dnn_finalize(void)
{
  return RT_RET_NOERROR;
}

int dnn_runtime_initialize(dnn_runtime_t * rt, const nn_network_t * network)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  DNN_CHECK_NULL_RET(network, -EINVAL);
  void *tmp_buf;

  /* register dnnrt's callback with rt_context */
  int err;
  err = (int)rt_allocate_context((rt_context_pointer *) & (rt->impl_ctx));
  if (err != RT_RET_NOERROR)
    {
      goto alloc_error;
    }
  rt_context_pointer ctx = (rt_context_pointer) (rt->impl_ctx);
  err =
    (int)rt_add_callback(ctx, NN_FUNCTION_CONVOLUTION, dnnrt_convolution_alloc);
  if (err != RT_RET_NOERROR)
    {
      goto error;
    }
  err = (int)rt_add_callback(ctx, NN_FUNCTION_AFFINE, dnnrt_affine_alloc);
  if (err != RT_RET_NOERROR)
    {
      goto error;
    }

  /* initialize rt_context and count up required minimum size of scratch_buf */
  s_dnn_gctx.req_scratch_buf_bsize = 0;
  /* remove const to use the as-is rt_initialize_context() */
  err = (int)rt_initialize_context(ctx, (nn_network_t *) network);
  if (err != RT_RET_NOERROR)
    {
      goto error;
    }

  /* resize scratch buffer */
  if (s_dnn_gctx.req_scratch_buf_bsize > s_dnn_gctx.scratch_buf_bsize)
    {
      tmp_buf =
        realloc(s_dnn_gctx.scratch_buf, s_dnn_gctx.req_scratch_buf_bsize);
      if (!tmp_buf)
        {
          err = -ENOMEM;
          goto error;
        }
      s_dnn_gctx.scratch_buf = tmp_buf;
      s_dnn_gctx.scratch_buf_bsize = s_dnn_gctx.req_scratch_buf_bsize;
    }
  ++s_dnn_gctx.rt_count;

  return 0;

error:
  rt_free_context(&rt->impl_ctx);
  rt->impl_ctx = NULL;
alloc_error:
  return err;
}

int dnn_runtime_finalize(dnn_runtime_t * rt)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);

  if (--s_dnn_gctx.rt_count == 0)
    {
      free(s_dnn_gctx.scratch_buf);
      s_dnn_gctx.scratch_buf = NULL;
      s_dnn_gctx.scratch_buf_bsize = 0;
      s_dnn_gctx.req_scratch_buf_bsize = 0;
    }

  return (int)rt_free_context((rt_context_pointer *) & (rt->impl_ctx));
}

int dnn_runtime_forward(dnn_runtime_t * rt, const void *inputs[],
                unsigned char input_num)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  rt_context_pointer ctx = (rt_context_pointer) rt->impl_ctx;
  if (rt_num_of_input (ctx) != input_num)
    {
      return -EINVAL;
    }
  rt_context_t *c = (rt_context_t *) ctx;

  for (int i = 0; i < input_num; ++i)
    {
      c->variables[c->input_variable_ids[i]].data = (void *)inputs[i];
    }

  return (int)rt_forward(ctx);
}

int dnn_runtime_input_num(dnn_runtime_t * rt)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  return rt_num_of_input((rt_context_pointer) rt->impl_ctx);
}

int dnn_runtime_input_size(dnn_runtime_t * rt, unsigned char data_index)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  return rt_input_size((rt_context_pointer) rt->impl_ctx, data_index);
}

int dnn_runtime_input_ndim(dnn_runtime_t * rt, unsigned char data_index)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  return rt_input_dimension((rt_context_pointer) rt->impl_ctx, data_index);
}

int
dnn_runtime_input_shape(dnn_runtime_t * rt, unsigned char data_index,
                        unsigned char dim_index)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  return rt_input_shape((rt_context_pointer) rt->impl_ctx, data_index,
                        dim_index);
}

void *dnn_input_buffer(dnn_runtime_t * rt, unsigned char data_index)
{
  DNN_CHECK_NULL_RET(rt, NULL);
  return rt_input_buffer(rt->impl_ctx, (size_t) data_index);
}

nn_variable_t *dnn_runtime_input_variable(dnn_runtime_t * rt,
                                          unsigned char data_index)
{
  DNN_CHECK_NULL_RET(rt, NULL);
  return rt_input_variable(rt->impl_ctx, data_index);
}

int dnn_runtime_output_num(dnn_runtime_t * rt)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  return rt_num_of_output(rt->impl_ctx);
}

int dnn_runtime_output_size(dnn_runtime_t * rt, unsigned char data_index)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  return rt_output_size(rt->impl_ctx, data_index);
}

int dnn_runtime_output_ndim(dnn_runtime_t * rt, unsigned char data_index)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  return rt_output_dimension(rt->impl_ctx, data_index);
}

int
dnn_runtime_output_shape(dnn_runtime_t * rt, unsigned char data_index,
                         unsigned char dim_index)
{
  DNN_CHECK_NULL_RET(rt, -EINVAL);
  return rt_output_shape(rt->impl_ctx, data_index, dim_index);
}

void *dnn_runtime_output_buffer(dnn_runtime_t * rt, unsigned char data_index)
{
  DNN_CHECK_NULL_RET(rt, NULL);
  return rt_output_buffer(rt->impl_ctx, (size_t) data_index);
}

nn_variable_t *dnn_runtime_output_variable(dnn_runtime_t * rt,
                                           unsigned char data_index)
{
  DNN_CHECK_NULL_RET(rt, NULL);
  return rt_output_variable(rt->impl_ctx, data_index);
}

dnn_global_context *dnn_get_global_context(void)
{
  return &s_dnn_gctx;
}

void dnn_req_scratch_buf(int size)
{
  if (size > s_dnn_gctx.req_scratch_buf_bsize)
    {
      s_dnn_gctx.req_scratch_buf_bsize = size;
    }
}

void *dnn_scratch_buf(void)
{
  return s_dnn_gctx.scratch_buf;
}
