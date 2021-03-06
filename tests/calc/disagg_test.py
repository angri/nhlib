# nhlib: A New Hazard Library
# Copyright (C) 2012 GEM Foundation
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
import unittest

import numpy

from nhlib.calc import disagg
from nhlib.calc import filters
from nhlib.tom import PoissonTOM
from nhlib.geo import Point, Mesh
from nhlib.site import Site


class _BaseDisaggTestCase(unittest.TestCase):
    class FakeSurface(object):
        def __init__(self, distance, lon, lat):
            self.distance = distance
            self.lon = lon
            self.lat = lat
        def get_joyner_boore_distance(self, sites):
            assert len(sites) == 1
            return numpy.array([self.distance], float)
        def get_closest_points(self, sites):
            assert len(sites) == 1
            return Mesh(numpy.array([self.lon], float),
                        numpy.array([self.lat], float),
                        depths=None)

    class FakeRupture(object):
        def __init__(self, mag, probability, distance, lon, lat):
            self.mag = mag
            self.probability = probability
            self.surface = _BaseDisaggTestCase.FakeSurface(distance, lon, lat)
        def get_probability_one_occurrence(self):
            return self.probability

    class FakeSource(object):
        def __init__(self, ruptures, tom, tectonic_region_type):
            self.ruptures = ruptures
            self.tom = tom
            self.tectonic_region_type = tectonic_region_type
        def iter_ruptures(self, tom):
            assert tom is self.tom
            return iter(self.ruptures)

    class FakeGSIM(object):
        def __init__(self, iml, imt, truncation_level, n_epsilons,
                     disaggregated_poes):
            self.disaggregated_poes = disaggregated_poes
            self.n_epsilons = n_epsilons
            self.iml = iml
            self.imt = imt
            self.truncation_level = truncation_level
            self.dists = object()
        def make_contexts(self, sites, rupture):
            return (sites, rupture, self.dists)
        def disaggregate_poe(self, sctx, rctx, dctx, imt, iml,
                             truncation_level, n_epsilons):
            assert truncation_level is self.truncation_level
            assert dctx is self.dists
            assert imt is self.imt
            assert iml is self.iml
            assert n_epsilons is self.n_epsilons
            assert len(sctx) == 1
            return numpy.array([self.disaggregated_poes[rctx]])

    def setUp(self):
        self.ruptures_and_poes1 = [
            ([0, 0, 0], self.FakeRupture(5, 0.1, 3, 22, 44)),
            ([0.1, 0.2, 0.1], self.FakeRupture(5, 0.2, 11, 22, 44)),
            ([0, 0, 0.3], self.FakeRupture(5, 0.01, 12, 22, 45)),
            ([0, 0.05, 0.001], self.FakeRupture(5, 0.33, 13, 22, 45)),
            ([0, 0, 0], self.FakeRupture(9, 0.4, 14, 21, 44)),
            ([0, 0, 0.02], self.FakeRupture(5, 0.05, 11, 21, 44)),
            ([0.04, 0.1, 0.04], self.FakeRupture(5, 0.53, 11, 21, 45)),
            ([0.2, 0.3, 0.2], self.FakeRupture(5, 0.066, 10, 21, 45)),
            ([0.3, 0.4, 0.3], self.FakeRupture(6, 0.1, 12, 22, 44)),
            ([0, 0, 0.1], self.FakeRupture(6, 0.1, 12, 21, 44)),
            ([0, 0, 0], self.FakeRupture(6, 0.1, 11, 22, 45)),
        ]
        self.ruptures_and_poes2 = [
            ([0, 0.1, 0.04], self.FakeRupture(8, 0.04, 5, 11, 45)),
            ([0.1, 0.5, 0.1], self.FakeRupture(7, 0.03, 5, 11, 46))
        ]
        self.tom = PoissonTOM(time_span=10)
        self.source1 = self.FakeSource(
            [rupture for poes, rupture in self.ruptures_and_poes1],
            self.tom, 'trt1'
        )
        self.source2 = self.FakeSource(
            [rupture for poes, rupture in self.ruptures_and_poes2],
            self.tom, 'trt2'
        )
        self.disagreggated_poes = dict(
            (rupture, poes)
            for (poes, rupture) in self.ruptures_and_poes1 \
                                   + self.ruptures_and_poes2
        )
        self.site = Site(Point(0, 0), 2, False, 4, 5)

        self.iml, self.imt, self.truncation_level = object(), object(), \
                                                    object()
        gsim = self.FakeGSIM(self.iml, self.imt, self.truncation_level,
                             n_epsilons=3,
                             disaggregated_poes=self.disagreggated_poes)
        self.gsim = gsim
        self.gsims = {'trt1': gsim, 'trt2': gsim}
        self.sources = [self.source1, self.source2]


class CollectBinsDataTestCase(_BaseDisaggTestCase):
    def test_no_filters(self):
        mags, dists, lons, \
        lats, joint_probs, trts, trt_bins = disagg._collect_bins_data(
            self.sources, self.site, self.imt, self.iml, self.gsims,
            self.tom, self.truncation_level, n_epsilons=3,
            source_site_filter=filters.source_site_noop_filter,
            rupture_site_filter=filters.rupture_site_noop_filter
        )

        aae = numpy.testing.assert_array_equal
        aaae = numpy.testing.assert_array_almost_equal

        aae(mags, [5, 5, 5, 5, 9, 5, 5, 5, 6, 6, 6, 8, 7])
        aae(dists, [3, 11, 12, 13, 14, 11, 11, 10, 12, 12, 11, 5, 5])
        aae(lons, [22, 22, 22, 22, 21, 21, 21, 21, 22, 21, 22, 11, 11])
        aae(lats, [44, 44, 45, 45, 44, 44, 45, 45, 44, 44, 45, 45, 46])
        aaae(joint_probs, [[0., 0., 0.],
                           [0.02, 0.04, 0.02],
                           [0., 0., 0.003],
                           [0., 0.0165, 0.00033],
                           [0., 0., 0.],
                           [0., 0., 0.001],
                           [0.0212, 0.053, 0.0212],
                           [0.0132, 0.0198, 0.0132],
                           [0.03, 0.04, 0.03],
                           [0., 0., 0.01],
                           [0., 0., 0.],
                           [0., 0.004, 0.0016],
                           [0.003, 0.015, 0.003]])
        aae(trts, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1])
        self.assertEqual(trt_bins, ['trt1', 'trt2'])

    def test_filters(self):
        def source_site_filter(sources_sites):
            for source, sites in sources_sites:
                if source is self.source2:
                    continue
                yield source, sites
        def rupture_site_filter(rupture_sites):
            for rupture, sites in rupture_sites:
                if rupture.mag < 6:
                    continue
                yield rupture, sites

        mags, dists, lons, \
        lats, joint_probs, trts, trt_bins = disagg._collect_bins_data(
            self.sources, self.site, self.imt, self.iml, self.gsims,
            self.tom, self.truncation_level, n_epsilons=3,
            source_site_filter=source_site_filter,
            rupture_site_filter=rupture_site_filter
        )

        aae = numpy.testing.assert_array_equal
        aaae = numpy.testing.assert_array_almost_equal

        aae(mags, [9, 6, 6, 6])
        aae(dists, [14, 12, 12, 11])
        aae(lons, [21, 22, 21, 22])
        aae(lats, [44, 44, 44, 45])
        aaae(joint_probs, [[0., 0., 0.],
                           [0.03, 0.04, 0.03],
                           [0., 0., 0.01],
                           [0., 0., 0.]])
        aae(trts, [0, 0, 0, 0])
        self.assertEqual(trt_bins, ['trt1'])


class DefineBinsTestCase(unittest.TestCase):
    def test(self):
        mags = numpy.array([4.4, 5, 3.2, 7, 5.9])
        dists = numpy.array([4, 1.2, 3.5, 52.1, 17])
        lats = numpy.array([-25, -10, 0.6, -20, -15])
        lons = numpy.array([179, -179, 176.4, -179.55, 180])
        joint_probs = None
        trts = [0, 1, 2, 2, 1]
        trt_bins = ['foo', 'bar', 'baz']

        bins_data = mags, dists, lons, lats, joint_probs, trts, trt_bins

        mag_bins, dist_bins, lon_bins, lat_bins, \
        eps_bins, trt_bins_ = disagg._define_bins(
            bins_data, mag_bin_width=1, dist_bin_width=4.2,
            coord_bin_width=1.2, truncation_level=1, n_epsilons=4
        )

        aae = numpy.testing.assert_array_equal
        aaae = numpy.testing.assert_array_almost_equal
        aae(mag_bins, [3, 4, 5, 6, 7])
        aaae(dist_bins, [0., 4.2, 8.4, 12.6, 16.8, 21., 25.2, 29.4, 33.6,
                         37.8, 42., 46.2, 50.4, 54.6])
        aaae(lon_bins, [176.4, 177.6, 178.8, -180., -178.8])
        aaae(lat_bins, [-25.2, -24., -22.8, -21.6, -20.4, -19.2, -18., -16.8,
                        -15.6, -14.4, -13.2, -12., -10.8, -9.6, -8.4, -7.2,
                        -6., -4.8, -3.6, -2.4, -1.2, 0., 1.2])
        aae(eps_bins, [-1., -0.5, 0., 0.5, 1.])
        self.assertIs(trt_bins, trt_bins_)


class ArangeDataInBinsTestCase(unittest.TestCase):
    def test(self):
        mags = numpy.array([5, 9, 5, 5, 9, 7, 5, 5, 6, 6, 9.2, 8, 7], float)
        dists = numpy.array([3, 1, 5, 13, 14, 6, 12, 10, 7, 4, 11, 13.4, 5],
                            float)
        lons = numpy.array([22, 21, 20, 21, 21, 22, 21, 21,
                            20.3, 21, 20.5, 21.5, 22], float)
        lats = numpy.array([44, 44, 45, 45, 44, 44, 45, 45,
                            44, 44, 45, 45, 43.3], float)
        joint_probs = numpy.array([[0., 0., 0.],
                                   [0.02, 0.04, 0.02],
                                   [0., 0., 0.003],
                                   [0., 0.0165, 0.00033],
                                   [0., 0., 0.],
                                   [0., 0., 0.001],
                                   [0.0212, 0.053, 0.0212],
                                   [0.0132, 0.0198, 0.0132],
                                   [0.03, 0.04, 0.03],
                                   [0., 0., 0.01],
                                   [0., 0., 0.],
                                   [0., 0.004, 0.0016],
                                   [0.003, 0.015, 0.003]])
        trts = numpy.array([0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1], int)
        trt_bins = ['trt1', 'trt2']

        bins_data = mags, dists, lons, lats, joint_probs, trts, trt_bins

        mag_bins = numpy.array([4, 6, 8, 10], float)
        dist_bins = numpy.array([0, 4, 8, 12, 16], float)
        lon_bins = numpy.array([19.2, 21, 22.8], float)
        lat_bins = numpy.array([43.2, 44.4, 45.6], float)
        eps_bins = numpy.array([-1.2, -0.4, 0.4, 1.2], float)

        bin_edges = mag_bins, dist_bins, lon_bins, lat_bins, eps_bins, trt_bins

        diss_matrix = disagg._arrange_data_in_bins(bins_data, bin_edges)

        self.assertEqual(diss_matrix.shape, (3, 4, 2, 2, 3, 2))
        self.assertAlmostEqual(diss_matrix.sum(), 1)

        for idx, value in [((0, 0, 0, 0, 2, 0), 0.0263831),
                           ((0, 1, 0, 0, 0, 0), 0.0791494),
                           ((0, 1, 0, 0, 1, 0), 0.1055325),
                           ((0, 1, 0, 0, 2, 0), 0.0791494),
                           ((0, 1, 0, 1, 2, 1), 0.0079149),
                           ((0, 2, 0, 1, 0, 0), 0.0559322),
                           ((0, 2, 0, 1, 0, 1), 0.0348257),
                           ((0, 2, 0, 1, 1, 0), 0.1398306),
                           ((0, 2, 0, 1, 1, 1), 0.0522386),
                           ((0, 2, 0, 1, 2, 0), 0.0559322),
                           ((0, 2, 0, 1, 2, 1), 0.0348257),
                           ((0, 3, 0, 1, 1, 1), 0.0435322),
                           ((0, 3, 0, 1, 2, 1), 0.0008706),
                           ((1, 1, 1, 0, 0, 1), 0.0079149),
                           ((1, 1, 1, 0, 1, 1), 0.0395747),
                           ((1, 1, 1, 0, 2, 1), 0.0105533),
                           ((1, 3, 1, 1, 1, 1), 0.0105533),
                           ((1, 3, 1, 1, 2, 1), 0.0042213),
                           ((2, 0, 0, 0, 0, 0), 0.0527663),
                           ((2, 0, 0, 0, 1, 0), 0.1055325),
                           ((2, 0, 0, 0, 2, 0), 0.0527663)]:
            self.assertAlmostEqual(diss_matrix[idx], value)
            diss_matrix[idx] = 0

        self.assertEqual(diss_matrix.sum(), 0)


class DisaggregateTestCase(_BaseDisaggTestCase):
    def test(self):
        self.gsim.truncation_level = self.truncation_level = 1
        bin_edges, matrix = disagg.disaggregation(
            self.sources, self.site, self.imt, self.iml, self.gsims,
            self.tom, self.truncation_level, n_epsilons=3,
            mag_bin_width=3, dist_bin_width=4, coord_bin_width=2.4
        )
        mag_bins, dist_bins, lon_bins, lat_bins, eps_bins, trt_bins = bin_edges
        aaae = numpy.testing.assert_array_almost_equal
        aaae(mag_bins, [3, 6, 9])
        aaae(dist_bins, [0, 4, 8, 12, 16])
        aaae(lon_bins, [9.6, 12., 14.4, 16.8, 19.2, 21.6, 24.])
        aaae(lat_bins, [43.2, 45.6, 48.])
        aaae(eps_bins, [-1, -0.3333333, 0.3333333, 1])
        self.assertEqual(trt_bins, ['trt1', 'trt2'])
        for idx, value in [((0, 2, 4, 0, 0, 0), 0.0907580),
                           ((0, 2, 4, 0, 1, 0), 0.1920692),
                           ((0, 2, 4, 0, 2, 0), 0.1197794),
                           ((0, 2, 5, 0, 0, 0), 0.1319157),
                           ((0, 2, 5, 0, 1, 0), 0.2110651),
                           ((0, 2, 5, 0, 2, 0), 0.1398306),
                           ((0, 3, 5, 0, 1, 0), 0.0435322),
                           ((0, 3, 5, 0, 2, 0), 0.0008706),
                           ((1, 1, 0, 0, 1, 1), 0.0105533),
                           ((1, 1, 0, 0, 2, 1), 0.0042213),
                           ((1, 1, 0, 1, 0, 1), 0.0079149),
                           ((1, 1, 0, 1, 1, 1), 0.0395747),
                           ((1, 1, 0, 1, 2, 1), 0.0079149)]:
            self.assertAlmostEqual(matrix[idx], value)
            matrix[idx] = 0

        self.assertEqual(matrix.sum(), 0)


class PMFExtractorsTestCase(unittest.TestCase):
    def setUp(self):
        super(PMFExtractorsTestCase, self).setUp()

        self.aae = numpy.testing.assert_almost_equal

        # test matrix is not normalized, but that's fine for test
        self.matrix = numpy.array(
        [ # magnitude
            [ # distance
                [ # longitude
                    [ # latitude
                        [ # epsilon
                            [0.00, 0.20, 0.50], # trt
                            [0.33, 0.44, 0.55],
                            [0.10, 0.11, 0.12]],
                        [
                            [0.60, 0.30, 0.20],
                            [0.50, 0.50, 0.30],
                            [0.00, 0.10, 0.20]]],
                    [
                        [
                            [0.10, 0.50, 0.78],
                            [0.15, 0.31, 0.21],
                            [0.74, 0.20, 0.95]],
                        [
                            [0.05, 0.82, 0.99],
                            [0.55, 0.02, 0.63],
                            [0.52, 0.49, 0.21]]]],
                [
                    [
                        [
                            [0.98, 0.59, 0.13],
                            [0.72, 0.40, 0.12],
                            [0.16, 0.61, 0.53]],
                        [
                            [0.04, 0.94, 0.84],
                            [0.13, 0.03, 0.31],
                            [0.95, 0.34, 0.31]]],
                    [
                        [
                            [0.25, 0.46, 0.34],
                            [0.79, 0.71, 0.17],
                            [0.5, 0.61, 0.7]],
                        [
                            [0.79, 0.15, 0.29],
                            [0.79, 0.14, 0.72],
                            [0.40, 0.84, 0.24]]]]],
            [
                [
                    [
                        [
                            [0.49, 0.73, 0.79],
                            [0.54, 0.20, 0.04],
                            [0.40, 0.32, 0.06]],
                        [
                            [0.73, 0.04, 0.60],
                            [0.53, 0.65, 0.71],
                            [0.47, 0.93, 0.70]]],
                    [
                        [
                            [0.32, 0.78, 0.97],
                            [0.75, 0.07, 0.59],
                            [0.03, 0.94, 0.12]],
                        [
                            [0.12, 0.15, 0.47],
                            [0.12, 0.62, 0.02],
                            [0.93, 0.13, 0.23]]]],
                [
                    [
                        [
                            [0.17, 0.14, 1.00],
                            [0.34, 0.27, 0.08],
                            [0.11, 0.85, 0.85]],
                        [
                            [0.76, 0.03, 0.86],
                            [0.97, 0.30, 0.80],
                            [0.67, 0.84, 0.41]]],
                    [
                        [
                            [0.27, 0.36, 0.96],
                            [0.52, 0.77, 0.35],
                            [0.39, 0.88, 0.20]],
                        [
                            [0.86, 0.17, 0.07],
                            [0.48, 0.44, 0.69],
                            [0.14, 0.61, 0.67]]]]]])

    def test_mag(self):
        pmf = disagg.mag_pmf(self.matrix)
        self.aae(pmf, [30.29, 34.57])

    def test_dist(self):
        pmf = disagg.dist_pmf(self.matrix)
        self.aae(pmf, [29.56, 35.3])

    def test_trt(self):
        pmf = disagg.trt_pmf(self.matrix)
        self.aae(pmf, [21.25, 21.03, 22.58])

    def test_mag_dist(self):
        pmf = disagg.mag_dist_pmf(self.matrix)
        self.aae(pmf, [[13.27, 17.02],
                       [16.29, 18.28]])

    def test_mag_dist_eps(self):
        pmf = disagg.mag_dist_eps_pmf(self.matrix)
        self.aae(pmf, [[[5.04, 4.49, 3.74],
                        [5.80, 5.03, 6.19]],
                       [[6.19, 4.84, 5.26],
                        [5.65, 6.01, 6.62]]])

    def test_lon_Lat(self):
        pmf = disagg.lon_lat_pmf(self.matrix)
        self.aae(pmf, [[13.97, 17.59],
                       [17.74, 15.56]])

    def test_mag_lon_lat(self):
        pmf = disagg.mag_lon_lat_pmf(self.matrix)
        self.aae(pmf, [[[6.59,  6.59],
                        [8.47,  8.64]],
                       [[7.38, 11.],
                        [9.27,  6.92]]])

    def test_lon_lat_trt(self):
        pmf = disagg.lon_lat_trt_pmf(self.matrix)
        self.aae(pmf, [[[4.34, 4.86, 4.77],
                        [6.35, 5.00, 6.24]],
                       [[4.81, 6.59, 6.34],
                        [5.75, 4.58, 5.23]]])
