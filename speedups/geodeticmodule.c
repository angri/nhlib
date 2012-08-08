#include <Python.h>
#include <numpy/arrayobject.h>
#include <numpy/npy_math.h>
#include <geos_c.h>
#include <math.h>
#include <stdio.h>

#define RAD(x) ((x) * M_PI / 180)
#define EARTH_RADIUS 6371


static PyObject *
geodetic_geodetic_distance(
        PyObject *self,
        PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {"lons1", "lats1", "lons2", "lats2", NULL};

    PyObject *plons1, *plats1, *plons2, *plats2;
    PyArray_Descr *double_dtype = NULL;
    NpyIter *iter = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "OOOO", kwlist,
                &plons1, &plats1, &plons2, &plats2))
        return NULL;

    PyArrayObject *lons1, *lats1, *lons2, *lats2;
    lons1 = (PyArrayObject *) PyArray_FromObject(plons1, NPY_DOUBLE, 0, 0);
    lats1 = (PyArrayObject *) PyArray_FromObject(plats1, NPY_DOUBLE, 0, 0);
    lons2 = (PyArrayObject *) PyArray_FromObject(plons2, NPY_DOUBLE, 0, 0);
    lats2 = (PyArrayObject *) PyArray_FromObject(plats2, NPY_DOUBLE, 0, 0);

    if (lons1 == NULL || lats1 == NULL || lons2 == NULL || lats2 == NULL)
        goto fail;


    PyArrayObject *op[5];
    npy_uint32 flags;
    npy_uint32 op_flags[5];
    NpyIter_IterNextFunc *iternext;
    double_dtype = PyArray_DescrFromType(NPY_DOUBLE);
    PyArray_Descr *op_dtypes[] = {
            double_dtype, double_dtype,
            double_dtype, double_dtype,
            double_dtype};
    char **dataptrarray;

    flags = 0;
    op[0] = lons1;
    op[1] = lats1;
    op[2] = lons2;
    op[3] = lats2;
    op[4] = NULL;
    op_flags[0] = op_flags[1] = op_flags[2] = op_flags[3] = NPY_ITER_READONLY;
    op_flags[4] = NPY_ITER_WRITEONLY | NPY_ITER_ALLOCATE;
    iter = NpyIter_MultiNew(
            5, op, flags, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags, op_dtypes);
    Py_DECREF(double_dtype);

    if (iter == NULL)
        goto fail;

    iternext = NpyIter_GetIterNext(iter, NULL);
    dataptrarray = NpyIter_GetDataPtrArray(iter);

    do
    {
        double register lon1 = RAD(*(double *) dataptrarray[0]);
        double register lat1 = RAD(*(double *) dataptrarray[1]);
        double register lon2 = RAD(*(double *) dataptrarray[2]);
        double register lat2 = RAD(*(double *) dataptrarray[3]);
        double *res = (double *) dataptrarray[4];

        *res = 2 * EARTH_RADIUS * asin(sqrt(
            pow(sin((lat1 - lat2) / 2.0), 2.0)
            + cos(lat1) * cos(lat2) * pow(sin((lon1 - lon2) / 2.0), 2.0)
        ));
    } while (iternext(iter));

    PyArrayObject *result = NpyIter_GetOperandArray(iter)[4];
    Py_INCREF(result);
    if (NpyIter_Deallocate(iter) != NPY_SUCCEED) {
        Py_DECREF(result);
        goto fail;
    }

    Py_DECREF(lons1);
    Py_DECREF(lats1);
    Py_DECREF(lons2);
    Py_DECREF(lats2);

    return (PyObject *) result;

fail:
    Py_XDECREF(lons1);
    Py_XDECREF(lats1);
    Py_XDECREF(lons2);
    Py_XDECREF(lats2);
    return NULL;
}

static PyObject *
geodetic_min_geodetic_distance(
        PyObject *self,
        PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {"mlons", "mlats", "slons", "slats", NULL};

    PyArrayObject *mlons, *mlats, *slons, *slats;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!O!O!O!", kwlist,
                &PyArray_Type, &mlons, &PyArray_Type, &mlats,
                &PyArray_Type, &slons, &PyArray_Type, &slats))
        return NULL;

    PyArray_Descr *double_dtype = PyArray_DescrFromType(NPY_DOUBLE);

    PyArrayObject *op_s[3], *op_m[2];
    npy_uint32 flags_s, flags_m;
    npy_uint32 op_flags_s[3], op_flags_m[2];
    NpyIter_IterNextFunc *iternext_s, *iternext_m;
    PyArray_Descr *op_dtypes_s[] = {double_dtype, double_dtype, double_dtype};
    PyArray_Descr *op_dtypes_m[] = {double_dtype, double_dtype};
    char **dataptrarray_s, **dataptrarray_m;


    flags_s = 0;
    op_s[0] = slons;
    op_s[1] = slats;
    op_s[2] = NULL;
    op_flags_s[0] = op_flags_s[1] = NPY_ITER_READONLY;
    op_flags_s[2] = NPY_ITER_WRITEONLY | NPY_ITER_ALLOCATE;
    NpyIter *iter_s = NpyIter_MultiNew(
            3, op_s, flags_s, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags_s, op_dtypes_s);
    if (iter_s == NULL) {
        Py_DECREF(double_dtype);
        return NULL;
    }
    iternext_s = NpyIter_GetIterNext(iter_s, NULL);
    dataptrarray_s = NpyIter_GetDataPtrArray(iter_s);

    flags_m = 0;
    op_m[0] = mlons;
    op_m[1] = mlats;
    op_flags_m[0] = op_flags_m[1] = NPY_ITER_READONLY;
    NpyIter *iter_m = NpyIter_MultiNew(
            2, op_m, flags_m, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags_m, op_dtypes_m);
    Py_DECREF(double_dtype);
    if (iter_m == NULL) {
        NpyIter_Deallocate(iter_s);
        return NULL;
    }
    iternext_m = NpyIter_GetIterNext(iter_m, NULL);
    dataptrarray_m = NpyIter_GetDataPtrArray(iter_m);

    do
    {
        double register slon = RAD(*(double *) dataptrarray_s[0]);
        double register slat = RAD(*(double *) dataptrarray_s[1]);
        double *res = (double *) dataptrarray_s[2];
        double register min_dist = INFINITY;

        do
        {
            // TODO: precompute mlons/mlats in radians, precompute cos(mlat)
            double register mlon = RAD(*(double *) dataptrarray_m[0]);
            double register mlat = RAD(*(double *) dataptrarray_m[1]);

            double register dist = asin(sqrt(
                pow(sin((mlat - slat) / 2.0), 2.0)
                + cos(mlat) * cos(slat) * pow(sin((mlon - slon) / 2.0), 2.0)
            ));
            if (dist < min_dist)
                min_dist = dist;

        } while (iternext_m(iter_m));
        NpyIter_Reset(iter_m, NULL);

        *res = min_dist * 2 * EARTH_RADIUS;
    } while (iternext_s(iter_s));

    PyArrayObject *result = NpyIter_GetOperandArray(iter_s)[2];
    Py_INCREF(result);
    if (NpyIter_Deallocate(iter_s) != NPY_SUCCEED
            || NpyIter_Deallocate(iter_m) != NPY_SUCCEED) {
        Py_DECREF(result);
        return NULL;
    }

    return (PyObject *) result;
}


static PyObject *
geodetic_min_distance(
        PyObject *self,
        PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {"mlons", "mlats", "mdepths",
                             "slons", "slats", "sdepths",
                             "indices", NULL};

    PyArrayObject *mlons, *mlats, *mdepths, *slons, *slats, *sdepths;
    unsigned char indices = 0;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!O!O!O!O!O!|b", kwlist,
                &PyArray_Type, &mlons, &PyArray_Type, &mlats,
                &PyArray_Type, &mdepths,
                &PyArray_Type, &slons, &PyArray_Type, &slats,
                &PyArray_Type, &sdepths,
                &indices))
        return NULL;

    PyArray_Descr *double_dtype = PyArray_DescrFromType(NPY_DOUBLE);
    PyArray_Descr *int_dtype = PyArray_DescrFromType(NPY_INT);

    PyArrayObject *op_s[4], *op_m[3];
    npy_uint32 flags_s, flags_m;
    npy_uint32 op_flags_s[4], op_flags_m[3];
    NpyIter_IterNextFunc *iternext_s, *iternext_m;
    PyArray_Descr *op_dtypes_s[] = {double_dtype, double_dtype, double_dtype,
                                    NULL /* result array */};
    if (indices)
        op_dtypes_s[3] = int_dtype;
    else
        op_dtypes_s[3] = double_dtype;

    PyArray_Descr *op_dtypes_m[] = {double_dtype, double_dtype, double_dtype};
    char **dataptrarray_s, **dataptrarray_m;


    flags_s = 0;
    op_s[0] = slons;
    op_s[1] = slats;
    op_s[2] = sdepths;
    op_s[3] = NULL;
    op_flags_s[0] = op_flags_s[1] = op_flags_s[2] = NPY_ITER_READONLY;
    op_flags_s[3] = NPY_ITER_WRITEONLY | NPY_ITER_ALLOCATE;
    NpyIter *iter_s = NpyIter_MultiNew(
            4, op_s, flags_s, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags_s, op_dtypes_s);
    if (iter_s == NULL) {
        Py_DECREF(double_dtype);
        Py_DECREF(int_dtype);
        return NULL;
    }
    iternext_s = NpyIter_GetIterNext(iter_s, NULL);
    dataptrarray_s = NpyIter_GetDataPtrArray(iter_s);

    flags_m = 0;
    op_m[0] = mlons;
    op_m[1] = mlats;
    op_m[2] = mdepths;
    op_flags_m[0] = op_flags_m[1] = op_flags_m[2] = NPY_ITER_READONLY;
    NpyIter *iter_m = NpyIter_MultiNew(
            3, op_m, flags_m, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags_m, op_dtypes_m);
    Py_DECREF(double_dtype);
    Py_DECREF(int_dtype);
    if (iter_m == NULL) {
        NpyIter_Deallocate(iter_s);
        return NULL;
    }
    iternext_m = NpyIter_GetIterNext(iter_m, NULL);
    dataptrarray_m = NpyIter_GetDataPtrArray(iter_m);

    do
    {
        double register slon = RAD(*(double *) dataptrarray_s[0]);
        double register slat = RAD(*(double *) dataptrarray_s[1]);
        double register sdepth = *(double *) dataptrarray_s[2];
        double register min_dist = INFINITY;
        int min_dist_idx = -1;

        do
        {
            // TODO: precompute mlons/mlats in radians, precompute cos(mlat)
            double register mlon = RAD(*(double *) dataptrarray_m[0]);
            double register mlat = RAD(*(double *) dataptrarray_m[1]);
            double register mdepth = *(double *) dataptrarray_m[2];

            double register geodetic_dist = asin(sqrt(
                pow(sin((mlat - slat) / 2.0), 2.0)
                + cos(mlat) * cos(slat) * pow(sin((mlon - slon) / 2.0), 2.0)
            )) * 2 * EARTH_RADIUS;
            double register vertical_dist = sdepth - mdepth;
            double register dist = sqrt(pow(geodetic_dist, 2)
                                        + pow(vertical_dist, 2));

            if (dist < min_dist) {
                min_dist = dist;
                if (indices)
                    min_dist_idx = NpyIter_GetIterIndex(iter_m);
            }

        } while (iternext_m(iter_m));
        NpyIter_Reset(iter_m, NULL);

        if (indices)
            *(int *) dataptrarray_s[3] = min_dist_idx;
        else
            *(double *) dataptrarray_s[3] = min_dist;
    } while (iternext_s(iter_s));

    PyArrayObject *result = NpyIter_GetOperandArray(iter_s)[3];
    Py_INCREF(result);
    if (NpyIter_Deallocate(iter_s) != NPY_SUCCEED
            || NpyIter_Deallocate(iter_m) != NPY_SUCCEED) {
        Py_DECREF(result);
        return NULL;
    }

    return (PyObject *) result;
}


static PyObject *
geodetic_point_to_polygon_distance(
        PyObject *self,
        PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {"mxx", "myy", "sxx", "syy", NULL};

    PyArrayObject *mxx, *myy, *sxx, *syy;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!O!O!O!", kwlist,
                &PyArray_Type, &mxx, &PyArray_Type, &myy,
                &PyArray_Type, &sxx, &PyArray_Type, &syy))
        return NULL;

    PyArray_Descr *double_dtype = PyArray_DescrFromType(NPY_DOUBLE);

    npy_uint32 flags;
    NpyIter_IterNextFunc *iternext;
    char **dataptrarray;

    PyArrayObject *op_m[2];
    npy_uint32 op_flags_m[2];
    PyArray_Descr *op_dtypes_m[2] = {double_dtype, double_dtype};

    flags = 0;
    op_m[0] = mxx;
    op_m[1] = myy;
    op_flags_m[0] = op_flags_m[1] = NPY_ITER_READONLY;
    NpyIter *iter = NpyIter_MultiNew(
            2, op_m, flags, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags_m, op_dtypes_m);
    if (iter == NULL) {
        Py_DECREF(double_dtype);
        return NULL;
    }
    iternext = NpyIter_GetIterNext(iter, NULL);
    dataptrarray = NpyIter_GetDataPtrArray(iter);

    GEOSGeometry **points = malloc(
            NpyIter_GetIterSize(iter) * sizeof(GEOSGeometry *));

    GEOSCoordSequence *coord_seq_m;

    double x, y;
    unsigned int i = 0;

    do
    {
        x = *(double *) dataptrarray[0];
        y = *(double *) dataptrarray[1];

        coord_seq_m = GEOSCoordSeq_create(1, 2);
        GEOSCoordSeq_setX(coord_seq_m, 0, x);
        GEOSCoordSeq_setY(coord_seq_m, 0, y);

        points[i] = GEOSGeom_createPoint(coord_seq_m);

        i++;
    } while (iternext(iter));

    GEOSGeometry *multipoint = GEOSGeom_createCollection(
            GEOS_MULTIPOINT, points, i);
    free(points);
    GEOSGeometry *polygon = GEOSConvexHull(multipoint);
    GEOSGeom_destroy(multipoint);

    PyArrayObject *op_s[3];
    npy_uint32 op_flags_s[3];
    PyArray_Descr *op_dtypes_s[3] = {double_dtype, double_dtype, double_dtype};

    flags = 0;
    op_s[0] = sxx;
    op_s[1] = syy;
    op_s[2] = NULL;
    op_flags_s[0] = op_flags_s[1] = NPY_ITER_READONLY;
    op_flags_s[2] = NPY_ITER_WRITEONLY | NPY_ITER_ALLOCATE;
    iter = NpyIter_MultiNew(
            3, op_s, flags, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags_s, op_dtypes_s);
    Py_DECREF(double_dtype);

    if (iter == NULL) {
        GEOSGeom_destroy(polygon);
        return NULL;
    }

    iternext = NpyIter_GetIterNext(iter, NULL);
    dataptrarray = NpyIter_GetDataPtrArray(iter);

    GEOSCoordSequence *coord_seq_s = GEOSCoordSeq_create(1, 2);
    GEOSGeometry *point = GEOSGeom_createPoint(coord_seq_s);

    do
    {
        double x = *(double *) dataptrarray[0];
        double y = *(double *) dataptrarray[1];
        double *dist = (double *) dataptrarray[2];

        GEOSCoordSeq_setX(coord_seq_s, 0, x);
        GEOSCoordSeq_setY(coord_seq_s, 0, y);

        GEOSDistance(polygon, point, dist);
    } while (iternext(iter));

    GEOSGeom_destroy(polygon);
    GEOSGeom_destroy(point);

    PyArrayObject *result = NpyIter_GetOperandArray(iter)[2];
    Py_INCREF(result);
    if (NpyIter_Deallocate(iter) != NPY_SUCCEED) {
        Py_DECREF(result);
        return NULL;
    }

    return (PyObject *) result;
}


static PyMethodDef GeodeticSpeedupsMethods[] = {
    {"geodetic_distance",
        (PyCFunction)geodetic_geodetic_distance,
        METH_VARARGS | METH_KEYWORDS,
        "TBD"},
    {"min_geodetic_distance",
        (PyCFunction)geodetic_min_geodetic_distance,
        METH_VARARGS | METH_KEYWORDS,
        "TBD"},
    {"min_distance",
        (PyCFunction)geodetic_min_distance,
        METH_VARARGS | METH_KEYWORDS,
        "TBD"},
    {"point_to_polygon_distance",
        (PyCFunction)geodetic_point_to_polygon_distance,
        METH_VARARGS | METH_KEYWORDS,
        "TBD"},

    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static void
message_handler(const char *fmt, ...) {
}


PyMODINIT_FUNC
init_geodetic_speedups(void)
{
    (void) Py_InitModule("_geodetic_speedups", GeodeticSpeedupsMethods);
    import_array();
    initGEOS(message_handler, message_handler);
}
