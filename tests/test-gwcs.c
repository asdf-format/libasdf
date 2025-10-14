#include <asdf.h>
#include <asdf/gwcs/fitswcs_imaging.h>
#include <asdf/gwcs/gwcs.h>

#include "asdf/gwcs/transform.h"
#include "munit.h"
#include "util.h"


MU_TEST(test_asdf_get_gwcs_fits) {
    const char *path = get_fixture_file_path("roman_wcs.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);

    asdf_gwcs_fits_t *fits = NULL;
    asdf_value_err_t err = asdf_get_gwcs_fits(file, "wcs/steps/0/transform", &fits);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(fits);

    // Cast to a generic transform and check the transform properties
    asdf_gwcs_transform_t *transform = (asdf_gwcs_transform_t *)fits;
    // Actually in this test case the transform is not given a name
    assert_int(transform->type, ==, ASDF_GWCS_TRANSFORM_FITSWCS_IMAGING);
    assert_null(transform->name);
    assert_not_null(transform->bounding_box);
    const asdf_gwcs_bounding_box_t *bb = transform->bounding_box;
    assert_string_equal(bb->intervals[0].input_name, "x");
    assert_double_equal(bb->intervals[0].bounds[0], -0.5, 9);
    assert_double_equal(bb->intervals[0].bounds[1], 4999.5, 9);
    assert_string_equal(bb->intervals[1].input_name, "y");
    assert_double_equal(bb->intervals[1].bounds[0], -0.5, 9);
    assert_double_equal(bb->intervals[1].bounds[1], 4999.5, 9);
    //TODO
    //assert_int(bb->order, ==, ASDF_ARRAY_STORAGE_ORDER_F);

    // Now check the FITS-specific properties
    double crpix[2] = {12099.5, -88700.5};
    double crval[2] = {270.0, 64.60237301};
    double cdelt[2] = {1.52777778e-05, 1.52777778e-05};
    double pc[2][2] = {{1.0, 0.0}, {-0.0, 1.0}};

    for (int idx = 0; idx < 2; idx++) {
        assert_double_equal(fits->crpix[idx], crpix[idx], 5);
        assert_double_equal(fits->crval[idx], crval[idx], 5);
        assert_double_equal(fits->cdelt[idx], cdelt[idx], 5);
        for (int jdx = 0; jdx < 2; jdx++) {
            assert_double_equal(fits->pc[idx][jdx], pc[idx][jdx], 5);
        }
    }

    assert_int(fits->projection.type, ==, ASDF_GWCS_TRANSFORM_GNOMONIC);
    asdf_gwcs_fits_destroy(fits);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_get_gwcs) {
    const char *path = get_fixture_file_path("roman_wcs.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);

    asdf_gwcs_t *gwcs = NULL;
    asdf_value_err_t err = asdf_get_gwcs(file, "wcs", &gwcs);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(gwcs);

    assert_string_equal(gwcs->name, "270p65x48y69");
    assert_int(gwcs->n_steps, ==, 2);
    assert_not_null(gwcs->steps);

    const asdf_gwcs_step_t *step = &gwcs->steps[0];
    assert_not_null(step->frame);
    assert_int(step->frame->type, ==, ASDF_GWCS_FRAME_2D);
    assert_string_equal(step->frame->name, "detector");
    asdf_gwcs_frame2d_t *frame2d = (asdf_gwcs_frame2d_t *)step->frame;
    assert_string_equal(frame2d->axes_names[0], "x");
    assert_string_equal(frame2d->axes_names[1], "y");
    assert_int(frame2d->axes_order[0], ==, 0);
    assert_int(frame2d->axes_order[1], ==, 1);

    step = &gwcs->steps[1];
    assert_not_null(step->frame);
    assert_int(step->frame->type, ==, ASDF_GWCS_FRAME_CELESTIAL);
    assert_string_equal(step->frame->name, "icrs");
    asdf_gwcs_frame_celestial_t *frame_celestial = (asdf_gwcs_frame_celestial_t *)step->frame;
    assert_string_equal(frame_celestial->axes_names[0], "lon");
    assert_string_equal(frame_celestial->axes_names[1], "lat");
    assert_null(frame_celestial->axes_names[2]);
    assert_int(frame_celestial->axes_order[0], ==, 0);
    assert_int(frame_celestial->axes_order[1], ==, 1);

    asdf_gwcs_destroy(gwcs);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_gwcs,
    MU_RUN_TEST(test_asdf_get_gwcs_fits),
    MU_RUN_TEST(test_asdf_get_gwcs)
);


MU_RUN_SUITE(test_asdf_gwcs);
