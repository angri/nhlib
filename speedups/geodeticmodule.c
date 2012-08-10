/*
 nhlib: A New Hazard Library
 Copyright (C) 2012 GEM Foundation

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation, either version 3 of the
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Python.h>
#include <numpy/arrayobject.h>
#include <numpy/npy_math.h>
#include <math.h>

#define EARTH_RADIUS 6371.0


/*
 * Calculate the distance between two points along the geodetic.
 * Parameters are two pairs of spherical coordinates in radians.
 */
static inline double
geodetic__geodetic_distance(double lon1, double lat1, double lon2, double lat2)
{
    return asin(sqrt(
        pow(sin((lat1 - lat2) / 2.0), 2.0)
        + cos(lat1) * cos(lat2) * pow(sin((lon1 - lon2) / 2.0), 2.0)
    )) * 2 * EARTH_RADIUS;
}


/*
 * Calculate the azimuth between two points. Parameters are two pairs
 * of spherical coordinates in radians.
 */
static inline double
geodetic__azimuth(double lon1, double lat1, double lon2, double lat2)
{
    double cos_lat2 = cos(lat2);
    double true_course = atan2(
        sin(lon1 - lon2) * cos_lat2,
        cos(lat1) * sin(lat2) - sin(lat1) * cos_lat2 * cos(lon1 - lon2)
    );
    return fmod(2 * M_PI - true_course, 2 * M_PI);
}


static const char geodetic_geodetic_distance__doc__[] = "\n\
    Calculate the geodetic distance between two collections of points,\n\
    following the numpy broadcasting rules.\n\
    \n\
    geodetic_distance(lons1, lats1, lons2, lats2) -> dists\n\
    \n\
    Parameters must be numpy arrays of double, representing spherical\n\
    coordinates in radians.\n\
";
static PyObject *
geodetic_geodetic_distance(
        PyObject *self,
        PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {"lons1", "lats1", "lons2", "lats2",
                             "azimuth", NULL};

    PyArrayObject *lons1, *lats1, *lons2, *lats2;
    unsigned char azimuth;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!O!O!O!b", kwlist,
                &PyArray_Type, &lons1, &PyArray_Type, &lats1,
                &PyArray_Type, &lons2, &PyArray_Type, &lats2, &azimuth))
        return NULL;

    PyArrayObject *op[5] = {lons1, lats1, lons2, lats2,
                            NULL /* distance or azimuth */};
    npy_uint32 flags = 0;
    npy_uint32 op_flags[5];
    NpyIter_IterNextFunc *iternext;
    PyArray_Descr *double_dtype = PyArray_DescrFromType(NPY_DOUBLE);
    PyArray_Descr *op_dtypes[] = {double_dtype, double_dtype,
                                  double_dtype, double_dtype,
                                  double_dtype};
    char **dataptrarray;

    op_flags[0] = op_flags[1] = op_flags[2] = op_flags[3] = NPY_ITER_READONLY;
    op_flags[4] = NPY_ITER_WRITEONLY | NPY_ITER_ALLOCATE;

    NpyIter *iter = NpyIter_MultiNew(
            5, op, flags, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags, op_dtypes);
    Py_DECREF(double_dtype);
    if (iter == NULL)
        return NULL;

    iternext = NpyIter_GetIterNext(iter, NULL);
    dataptrarray = NpyIter_GetDataPtrArray(iter);
    do
    {
        double lon1 = *(double *) dataptrarray[0];
        double lat1 = *(double *) dataptrarray[1];
        double lon2 = *(double *) dataptrarray[2];
        double lat2 = *(double *) dataptrarray[3];
        if (!azimuth)
            *(double *) dataptrarray[4] = geodetic__geodetic_distance(
                    lon1, lat1, lon2, lat2);
        else
            *(double *) dataptrarray[4] = geodetic__azimuth(
                    lon1, lat1, lon2, lat2);
    } while (iternext(iter));

    PyArrayObject *result = NpyIter_GetOperandArray(iter)[4];
    Py_INCREF(result);
    if (NpyIter_Deallocate(iter) != NPY_SUCCEED) {
        Py_DECREF(result);
        return NULL;
    }

    return (PyObject *) result;
}


static const char geodetic_min_distance__doc__[] = "\n\
    Calculate the minimum distance between two collections of points.\n\
    \n\
    min_distance(mlons, mlats, mdepths, slons, \\\n\
            slats, sdepths, indices) -> min distances or indices of such\n\
    \n\
    Parameters mlons, mlats and mdepths represent coordinates of the first\n\
    collection of points and slons, slats, sdepths are for the second one.\n\
    All the coordinates must be numpy arrays of double. Longitudes and \n\
    latitudes are in radians, depths are in km.\n\
    \n\
    Boolean parameter \"indices\" determines whether actual minimum\n\
    distances should be returned or integer indices of closest points\n\
    from first collection. Thus, result is numpy array of either distances\n\
    (array of double, when indices=False) or indices (array of integers,\n\
    when indices=True).\n\
";
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

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!O!O!O!O!O!b", kwlist,
                &PyArray_Type, &mlons, &PyArray_Type, &mlats,
                &PyArray_Type, &mdepths,
                &PyArray_Type, &slons, &PyArray_Type, &slats,
                &PyArray_Type, &sdepths,
                &indices))
        return NULL;

    PyArray_Descr *double_dtype = PyArray_DescrFromType(NPY_DOUBLE);
    PyArray_Descr *int_dtype = PyArray_DescrFromType(NPY_INT);

    PyArrayObject *op_s[4] = {slons, slats, sdepths, NULL /* min distance */};
    PyArrayObject *op_m[3] = {mlons, mlats, mdepths};
    npy_uint32 flags_s = 0, flags_m = 0;
    npy_uint32 op_flags_s[4], op_flags_m[3];
    NpyIter_IterNextFunc *iternext_s, *iternext_m;
    PyArray_Descr *op_dtypes_s[] = {double_dtype, double_dtype, double_dtype,
                                    indices ? int_dtype : double_dtype};

    PyArray_Descr *op_dtypes_m[] = {double_dtype, double_dtype, double_dtype};
    char **dataptrarray_s, **dataptrarray_m;

    op_flags_s[0] = op_flags_s[1] = op_flags_s[2] = NPY_ITER_READONLY;
    op_flags_s[3] = NPY_ITER_WRITEONLY | NPY_ITER_ALLOCATE;

    NpyIter *iter_s = NpyIter_MultiNew(
            4, op_s, flags_s, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags_s, op_dtypes_s);
    Py_DECREF(int_dtype);
    if (iter_s == NULL) {
        Py_DECREF(double_dtype);
        return NULL;
    }
    iternext_s = NpyIter_GetIterNext(iter_s, NULL);
    dataptrarray_s = NpyIter_GetDataPtrArray(iter_s);

    op_flags_m[0] = op_flags_m[1] = op_flags_m[2] = NPY_ITER_READONLY;

    NpyIter *iter_m = NpyIter_MultiNew(
            3, op_m, flags_m, NPY_KEEPORDER, NPY_NO_CASTING,
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
        double slon = *(double *) dataptrarray_s[0];
        double slat = *(double *) dataptrarray_s[1];
        double sdepth = *(double *) dataptrarray_s[2];
        double min_dist = INFINITY;
        int min_dist_idx = -1;

        do
        {
            double mlon = *(double *) dataptrarray_m[0];
            double mlat = *(double *) dataptrarray_m[1];
            double mdepth = *(double *) dataptrarray_m[2];

            double geodetic_dist = geodetic__geodetic_distance(
                mlon, mlat, slon, slat);

            double vertical_dist = sdepth - mdepth;
            double dist;
            if (vertical_dist == 0)
                dist = geodetic_dist;
            else
                dist = sqrt(geodetic_dist * geodetic_dist
                            + vertical_dist * vertical_dist);

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


static const char geodetic_distance_to_arc__doc__[] = "\n\
    Calculate a closest distance between a great circle arc and a point\n\
    (or a collection of points).\n\
    \n\
    distance_to_arc(alons, alats, aazimuths, plons, plats) -> dists\n\
";
static PyObject *
geodetic_distance_to_arc(
        PyObject *self,
        PyObject *args,
        PyObject *keywds)
{
    static char *kwlist[] = {"alons", "alats", "aazimuths",
                             "plons", "plats", NULL};

    PyArrayObject *alats, *alons, *aazimuths, *plons, *plats;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O!O!O!O!O!", kwlist,
                &PyArray_Type, &alons, &PyArray_Type, &alats,
                &PyArray_Type, &aazimuths,
                &PyArray_Type, &plons, &PyArray_Type, &plats))
        return NULL;

    PyArrayObject *op[6] = {alons, alats, aazimuths, plons, plats,
                            NULL /* distance */};
    npy_uint32 flags = 0;
    npy_uint32 op_flags[6];
    NpyIter_IterNextFunc *iternext;
    PyArray_Descr *double_dtype = PyArray_DescrFromType(NPY_DOUBLE);
    PyArray_Descr *op_dtypes[] = {double_dtype, double_dtype,
                                  double_dtype, double_dtype,
                                  double_dtype, double_dtype};
    char **dataptrarray;

    op_flags[0] = op_flags[1] = op_flags[2] \
                  = op_flags[3] = op_flags[4] = NPY_ITER_READONLY;
    op_flags[5] = NPY_ITER_WRITEONLY | NPY_ITER_ALLOCATE;

    NpyIter *iter = NpyIter_MultiNew(
            6, op, flags, NPY_KEEPORDER, NPY_NO_CASTING,
            op_flags, op_dtypes);
    Py_DECREF(double_dtype);
    if (iter == NULL)
        return NULL;

    iternext = NpyIter_GetIterNext(iter, NULL);
    dataptrarray = NpyIter_GetDataPtrArray(iter);
    do
    {
        double alon = *(double *) dataptrarray[0];
        double alat = *(double *) dataptrarray[1];
        double aazimuth = *(double *) dataptrarray[2];
        double plon = *(double *) dataptrarray[3];
        double plat = *(double *) dataptrarray[4];

        double azimuth_to_target = geodetic__azimuth(alon, alat, plon, plat);
        double dist_to_target = geodetic__geodetic_distance(alon, alat,
                                                            plon, plat);

        double t_angle = fmod(azimuth_to_target - aazimuth + 2 * M_PI,
                              2 * M_PI);

        double cos_angle = sin(t_angle) * sin(dist_to_target / EARTH_RADIUS);
        if (cos_angle < -1)
            cos_angle = -1;
        else if (cos_angle > 1)
            cos_angle = 1;

        *(double *) dataptrarray[5] = (M_PI / 2 - acos(cos_angle)) \
                                      * EARTH_RADIUS;
    } while (iternext(iter));

    PyArrayObject *result = NpyIter_GetOperandArray(iter)[5];
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
            geodetic_geodetic_distance__doc__},
    {"min_distance",
            (PyCFunction)geodetic_min_distance,
            METH_VARARGS | METH_KEYWORDS,
            geodetic_min_distance__doc__},
    {"distance_to_arc",
            (PyCFunction)geodetic_distance_to_arc,
            METH_VARARGS | METH_KEYWORDS,
            geodetic_distance_to_arc__doc__},

    {NULL, NULL, 0, NULL} /* Sentinel */
};


PyMODINIT_FUNC
init_geodetic_speedups(void)
{
    (void) Py_InitModule("_geodetic_speedups", GeodeticSpeedupsMethods);
    import_array();
}
