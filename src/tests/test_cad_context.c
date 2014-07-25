/*   
 * This file is part of cf4ocl (C Framework for OpenCL).
 * 
 * cf4ocl is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * cf4ocl is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with cf4ocl. If not, see <http://www.gnu.org/licenses/>.
 * */

/** 
 * @file
 * @brief Create, add, destroy tests for context wrappers. Also tests
 * device selection filters, device wrappers and platform wrappers.
 * 
 * @author Nuno Fachada
 * @date 2014
 * @copyright [GNU General Public License version 3 (GPLv3)](http://www.gnu.org/licenses/gpl.html)
 * */

#include "platforms.h"
#include "platform_wrapper.h"
#include "device_wrapper.h"
#include "device_query.h"
#include "context_wrapper.h"
#include "common.h"
#include "program_wrapper.h"
#include "memobj_wrapper.h"
#include "buffer_wrapper.h"
#include <glib/gstdio.h>

/*
 * Independent pass-all filter for testing.
 * */
static gboolean cl4_devsel_indep_test_true(
	CL4Device* device, void *data, GError **err) {

	device = device;
	data = data;
	err = err;
	return TRUE;

}

/**
 * @brief Tests creation, getting info from and destruction of 
 * context wrapper objects.
 * */
static void context_create_info_destroy_test() {
	
	CL4Context* ctx = NULL;
	GError* err = NULL;
	CL4Platforms* ps = NULL;
	CL4Platform* p = NULL;
	CL4Device* d = NULL;
	cl_device_id d_id = NULL;
	CL4DevSelFilters filters = NULL;
	CL4WrapperInfo* info = NULL;
	cl_context_properties* ctx_props = NULL;
	cl_platform_id platform, platf_ref;
	cl_context context;
	cl_device_type device_type;
	guint num_devices;
	cl_int ocl_status;
	gboolean any_device;
	
	/* 
	 * 1. Test context creation from cl_devices. 
	 * */
	 
	/* Get platforms object. */
	ps = cl4_platforms_new(&err);
	g_assert_no_error(err);

	/* Get first platform wrapper from platforms object. */
	p = cl4_platforms_get_platform(ps, 0);
	g_assert(p != NULL);
	
	/* Get first device wrapper from platform wrapper. */
	d = cl4_platform_get_device(p, 0, &err);
	g_assert_no_error(err);
	
	/* Unwrap cl_device_id from device wrapper object. */
	d_id = cl4_device_unwrap(d);
	
	/* Create a context from this cl_device_id. */
	ctx = cl4_context_new_from_cldevices(1, &d_id, &err);
	g_assert_no_error(err);
	
	/* Get number of devices from context wrapper, check that this
	 * number is 1. */
#ifdef CL_VERSION_1_1
	info = cl4_context_info(ctx, CL_CONTEXT_NUM_DEVICES, &err);
	g_assert_no_error(err);
	g_assert_cmpuint(*((cl_uint*) info->value), ==, 1);
#endif
	
	/* Get the cl_device_id from context via context info and check
	 * that it corresponds to the cl_device_id with which the context
	 * was created. */
	info = cl4_context_info(ctx, CL_CONTEXT_DEVICES, &err);
	g_assert_no_error(err);
	g_assert(((cl_device_id*) info->value)[0] == d_id);

	/* Check again that the number of devices is 1, this time not using
	 * CL_CONTEXT_NUM_DEVICES, which is not available in OpenCL 1.0. */
	g_assert_cmpuint(info->size / sizeof(cl_device_id), ==, 1);

	/* Free context. */
	cl4_context_destroy(ctx);

	/* 
	 * 2. Test context creation by cl_context. 
	 * */
	
	/* Create some context properties. */
	ctx_props = g_new0(cl_context_properties, 3);
	platform = (cl_platform_id) cl4_wrapper_unwrap((CL4Wrapper*) p);
	ctx_props[0] = CL_CONTEXT_PLATFORM;
	ctx_props[1] = (cl_context_properties) platform;
	ctx_props[2] = 0;

	/* Create a CL context. */
	context = clCreateContext(
		(const cl_context_properties*) ctx_props, 
		1, &d_id, NULL, NULL, &ocl_status);
	g_assert_cmpint(ocl_status, ==, CL_SUCCESS);
	
	/* Create a context wrapper using the cl_context, check that the
	 * unwrapped cl_context corresponds to the cl_context with which
	 * the context wrapper was created.*/
	ctx = cl4_context_new_wrap(context);
	g_assert(cl4_context_unwrap(ctx) == context);
	
	/* Get the first device wrapper from the context wrapper, check that 
	 * the unwrapped cl_device_id corresponds to the cl_device_id with
	 * which the cl_context was created. */
	d = cl4_context_get_device(ctx, 0, &err);
	g_assert_no_error(err);
	g_assert(cl4_device_unwrap(d) == d_id);

	/* Check that the context number of devices taken using context
	 * info is 1. */
#ifdef CL_VERSION_1_1
	info = cl4_context_info(ctx, CL_CONTEXT_NUM_DEVICES, &err);
	g_assert_cmpuint(*((cl_uint*) info->value), ==, 1);
#else
	info = cl4_context_info(ctx, CL_CONTEXT_DEVICES, &err);
	g_assert_cmpuint(info->size / sizeof(cl_device_id), ==, 1);
#endif

	/* Free context, platforms and context properties. */
	cl4_context_destroy(ctx);
	cl4_platforms_destroy(ps);
	g_free(ctx_props);
	
	/* 
	 * 3. Test context creation by device filtering 
	 * (using shortcut macros). 
	 * */
	 
	/* For the next device type filters, at least one device must be
	 * found in order the test to pass. */
	any_device = FALSE;
	 
	/* 3.1. GPU device type filter. */
		
	/* Check that either there was no error or that no GPU was found. */
	ctx = cl4_context_new_gpu(&err);
	g_assert((err == NULL) || (err->code == CL4_ERROR_DEVICE_NOT_FOUND));
	any_device |= (ctx != NULL);

	/* Free context if no error and set filters to NULL. */
	if (err != NULL) {
		g_test_message("%s", err->message);
		g_clear_error(&err);
	} else { 
		cl4_context_destroy(ctx);
	}
	filters = NULL;

	/* 3.2. CPU device type filter. */

	/* Check that either there was no error or that no CPU was found. */
	ctx = cl4_context_new_cpu(&err);
	g_assert((err == NULL) || (err->code == CL4_ERROR_DEVICE_NOT_FOUND));
	any_device |= (ctx != NULL);

	/* Free context if no error and set filters to NULL. */
	if (err != NULL) {
		g_test_message("%s", err->message);
		g_clear_error(&err);
	} else { 
		cl4_context_destroy(ctx);
	}
	filters = NULL;

	/* 3.3. Accel. device type filter. */
	
	/* Check that either there was no error or that no accelerator was 
	 * found. */
	ctx = cl4_context_new_accel(&err);
	g_assert((err == NULL) || (err->code == CL4_ERROR_DEVICE_NOT_FOUND));
	any_device |= (ctx != NULL);

	/* Free context if no error and set filters to NULL. */
	if (err != NULL) {
		g_test_message("%s", err->message);
		g_clear_error(&err);
	} else { 
		cl4_context_destroy(ctx);
	}
	filters = NULL;
	
	/* Check that at least one device type context was created. */
	g_assert(any_device);
	
	/* 3.4. Specific platform filter. */

	/* Check that a context wrapper was created. */
	ctx = cl4_context_new_from_indep_filter(
		cl4_devsel_indep_platform, (void*) platform, &err);
	g_assert_no_error(err);
	
	/* Check that context wrapper contains a device. */
	d = cl4_context_get_device(ctx, 0, &err);
	g_assert_no_error(err);
	
	/* Check that the device platform corresponds to the expected 
	 * platform (the one used in the filter). */
	platf_ref = cl4_device_info_value_scalar(d, CL_DEVICE_PLATFORM,
		cl_platform_id, &err);
	g_assert_no_error(err);
	g_assert(platf_ref == platform);
	
	/* Free context and set filters to NULL. */
	cl4_context_destroy(ctx);
	filters = NULL;
	
	/* 
	 * 4. Test context creation by device filtering 
	 * (explicit dependent filters). 
	 * */
	 
	/* Same platform filter. */
	cl4_devsel_add_dep_filter(&filters, cl4_devsel_dep_platform, NULL);

	/* Check that a context wrapper was created. */
	ctx = cl4_context_new_from_filters(&filters, &err);
	g_assert_no_error(err);
	
	/* Check that context wrapper contains a device. */
	d = cl4_context_get_device(ctx, 0, &err);
	g_assert_no_error(err);
	
	/* Check that the device platform corresponds to the expected 
	 * platform (the one which the first device belongs to). */
	platf_ref = cl4_device_info_value_scalar(d, CL_DEVICE_PLATFORM,
		cl_platform_id, &err);
	g_assert_no_error(err);
	
	/* Get number of devices. */
	num_devices = cl4_context_get_num_devices(ctx, &err);
	g_assert_no_error(err);
	
	/* Check that all devices belong to the same platform. */
	for (guint i = 1; i < num_devices; i++) {
		
		d = cl4_context_get_device(ctx, i, &err);
		g_assert_no_error(err);
		
		platform = cl4_device_info_value_scalar(d, CL_DEVICE_PLATFORM,
			cl_platform_id, &err);
		g_assert_no_error(err);
			
		g_assert(platf_ref == platform);
	}
	
	/* Free context and set filters to NULL. */
	cl4_context_destroy(ctx);
	filters = NULL;

	/* 
	 * 5. Test context creation by device filtering 
	 * (explicit independent and dependent filters). 
	 * */
	
	/* Add pass all independent filter for testing. */
	cl4_devsel_add_indep_filter(
		&filters, cl4_devsel_indep_test_true, NULL);
		
	/* Add another pass all independent filter by manipulating the
	 * cl4_devsel_indep_type() filter. */
	device_type = CL_DEVICE_TYPE_ALL;
	cl4_devsel_add_indep_filter(
		&filters, cl4_devsel_indep_type, &device_type);
	
	/* Add same platform dependent filter. */
	cl4_devsel_add_dep_filter(&filters, cl4_devsel_dep_platform, NULL);

	/* Create context wrapper, which must have at least one device. */
	ctx = cl4_context_new_from_filters(&filters, &err);
	g_assert_no_error(err);
	
	num_devices = cl4_context_get_num_devices(ctx, &err);
	g_assert_no_error(err);
	g_assert_cmpuint(num_devices, >, 0);

	/* Free context and set filters to NULL. */
	cl4_context_destroy(ctx);
	filters = NULL;

}

/** 
 * @brief Test increasing reference count of objects which compose 
 * larger objects, then destroy the larger object and verify that 
 * composing object still exists and must be freed by the function
 * which increase its reference count.
 * 
 * This function tests the following modules: context, device and 
 * platform.
 * */
static void context_ref_unref_test() {

	CL4Context* ctx = NULL;
	GError* err = NULL;
	CL4Platforms* ps = NULL;
	CL4Platform* p = NULL;
	CL4Device* d = NULL;
	cl_device_id d_id = NULL;
	CL4DevSelFilters filters = NULL;
	
	/* Test context creating from cl_devices. */
	ps = cl4_platforms_new(&err);
	g_assert_no_error(err);

	p = cl4_platforms_get_platform(ps, 0);
	g_assert(p != NULL);
	
	d = cl4_platform_get_device(p, 0, &err);
	g_assert_no_error(err);
	
	d_id = cl4_device_unwrap(d);
		
	ctx = cl4_context_new_from_cldevices(1, &d_id, &err);
	g_assert_no_error(err);
	
	g_assert_cmpuint(cl4_wrapper_ref_count((CL4Wrapper*) d), ==, 1);
	g_assert_cmpuint(cl4_wrapper_ref_count((CL4Wrapper*) ctx), ==, 1);
	
	cl4_context_ref(ctx);
	g_assert_cmpuint(cl4_wrapper_ref_count((CL4Wrapper*) ctx), ==, 2);
	cl4_context_unref(ctx);
	g_assert_cmpuint(cl4_wrapper_ref_count((CL4Wrapper*) ctx), ==, 1);
	
	cl4_platforms_destroy(ps);
	cl4_context_destroy(ctx);

	/* Test context creating by device filtering. */
	cl4_devsel_add_indep_filter(&filters, cl4_devsel_indep_type_gpu, NULL);
	cl4_devsel_add_dep_filter(&filters, cl4_devsel_dep_platform, NULL);	
	
	ctx = cl4_context_new_from_filters(&filters, &err);
	g_assert((err == NULL) || (err->code == CL4_ERROR_DEVICE_NOT_FOUND));

	if (err != NULL) {
		g_test_message("%s", err->message);
		g_clear_error(&err);
	} else {
		g_assert_cmpuint(cl4_wrapper_ref_count((CL4Wrapper*) ctx), ==, 1);
		cl4_context_destroy(ctx);
	}
	filters = NULL;

	cl4_devsel_add_indep_filter(&filters, cl4_devsel_indep_type_cpu, NULL);
	cl4_devsel_add_dep_filter(&filters, cl4_devsel_dep_platform, NULL);
	
	ctx = cl4_context_new_from_filters(&filters, &err);
	g_assert((err == NULL) || (err->code == CL4_ERROR_DEVICE_NOT_FOUND));

	if (err != NULL) {
		g_test_message("%s", err->message);
		g_clear_error(&err);
	} else {
		g_assert_cmpuint(cl4_wrapper_ref_count((CL4Wrapper*) ctx), ==, 1);
		cl4_context_destroy(ctx);
	}
	filters = NULL;

	/// @todo Test context from CL4Platform, check that devices have ref=2 (kept by CL4Platform and CL4Context)
	/// @todo Test ref/unref program
	/// @todo Test ref/unref queue
	/// @todo Test ref/unref kernels

}

/**
 * @brief Main function.
 * @param argc Number of command line arguments.
 * @param argv Command line arguments.
 * @return Result of test run.
 * */
int main(int argc, char** argv) {

	g_test_init(&argc, &argv, NULL);

	g_test_add_func(
		"/wrappers/context/create-info-destroy", 
		context_create_info_destroy_test);

	g_test_add_func(
		"/wrappers/context/ref-unref", 
		context_ref_unref_test);
		
	return g_test_run();
}


