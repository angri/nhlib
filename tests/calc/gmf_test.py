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
from scipy.stats import truncnorm
from numpy.testing import assert_allclose, assert_array_equal

from nhlib import const
from nhlib.imt import SA, PGV
from nhlib.site import Site, SiteCollection
from nhlib.geo import Point
from nhlib.calc.gmf import ground_motion_fields


class GMFCalcNoCorrelationTestCase(unittest.TestCase):
    def setUp(self):
        self.mean1 = 1
        self.mean2 = 5
        self.mean3 = 10
        self.mean4567 = 10
        self.inter1 = 0.4
        self.inter2 = 1
        self.inter3 = 1.4
        self.inter45 = 1e-300
        self.inter67 = 1
        self.intra1 = 0.7
        self.intra2 = 2
        self.intra3 = 0.3
        self.intra45 = 1
        self.intra67 = 1e-300
        self.stddev1 = (self.inter1 ** 2 + self.intra1 ** 2) ** 0.5
        self.stddev2 = (self.inter2 ** 2 + self.intra2 ** 2) ** 0.5
        self.stddev3 = (self.inter3 ** 2 + self.intra3 ** 2) ** 0.5
        self.stddev45 = (self.inter45 ** 2 + self.intra45 ** 2) ** 0.5
        self.stddev67 = (self.inter67 ** 2 + self.intra67 ** 2) ** 0.5
        p0 = Point(0, 0)
        sites = [Site(p0, self.mean1, True, self.inter1, self.intra1),
                 Site(p0, self.mean2, True, self.inter2, self.intra2),
                 Site(p0, self.mean3, True, self.inter3, self.intra3),
                 Site(p0, self.mean4567, True, self.inter45, self.intra45),
                 Site(p0, self.mean4567, True, self.inter45, self.intra45),
                 Site(p0, self.mean4567, True, self.inter67, self.intra67),
                 Site(p0, self.mean4567, True, self.inter67, self.intra67)]
        self.sites = SiteCollection(sites)
        self.rupture = object()
        self.imt1 = SA(10, 5)
        self.imt2 = PGV()

        class FakeGSIM(object):
            def make_contexts(gsim, sites, rupture):
                self.assertIs(sites, self.sites)
                self.assertIs(rupture, self.rupture)
                return sites.vs30, sites.z1pt0, sites.z2pt5

            def get_mean_and_stddevs(gsim, mean, std_inter, std_intra, imt,
                                     stddev_types):
                assert imt is self.imt1 or imt is self.imt2
                self.assertEqual(stddev_types, [const.StdDev.INTER_EVENT,
                                                const.StdDev.INTRA_EVENT])
                return mean, [std_inter, std_intra]

        self.gsim = FakeGSIM()

    def test_no_filtering_no_truncation(self):
        truncation_level = None
        numpy.random.seed(3)
        num_samples = 400
        intensity = []
        for i in xrange(num_samples):
            gmfs = ground_motion_fields(self.rupture, self.sites,
                                        [self.imt1, self.imt2], self.gsim,
                                        truncation_level)
            intensity.append(gmfs[self.imt1])
            intensity.append(gmfs[self.imt2])
        intensity = numpy.array(intensity).transpose()

        assert_allclose((intensity[0].mean(), intensity[0].std()),
                        (self.mean1, self.stddev1), rtol=5e-2)
        assert_allclose((intensity[1].mean(), intensity[1].std()),
                        (self.mean2, self.stddev2), rtol=5e-2)
        assert_allclose((intensity[2].mean(), intensity[2].std()),
                        (self.mean3, self.stddev3), rtol=5e-2)

        assert_allclose((intensity[3].mean(), intensity[3].std()),
                        (self.mean4567, self.stddev45), rtol=5e-2)
        assert_allclose((intensity[4].mean(), intensity[4].std()),
                        (self.mean4567, self.stddev45), rtol=5e-2)

        assert_allclose((intensity[5].mean(), intensity[5].std()),
                        (self.mean4567, self.stddev67), rtol=5e-2)
        assert_allclose((intensity[6].mean(), intensity[6].std()),
                        (self.mean4567, self.stddev67), rtol=5e-2)

        # sites with zero inter-event stddev, should give exactly the same
        # result, since intra-event distribution is sampled only once
        assert_array_equal(intensity[3], intensity[4])

        self.assertFalse((intensity[5] == intensity[6]).all())

    def test_no_filtering_with_truncation(self):
        truncation_level = 1.9
        numpy.random.seed(11)
        num_samples = 400
        intensity = []
        for i in xrange(num_samples):
            gmfs = ground_motion_fields(self.rupture, self.sites,
                                        [self.imt1, self.imt2], self.gsim,
                                        truncation_level)
            intensity.append(gmfs[self.imt1])
            intensity.append(gmfs[self.imt2])

        intensity = numpy.array(intensity).transpose()

        max_deviation1 = (self.inter1 + self.intra1) * truncation_level
        max_deviation2 = (self.inter2 + self.intra2) * truncation_level
        max_deviation3 = (self.inter3 + self.intra3) * truncation_level
        max_deviation4567 = truncation_level
        self.assertLessEqual(intensity[0].max(), self.mean1 + max_deviation1)
        self.assertGreaterEqual(intensity[0].min(),
                                self.mean1 - max_deviation1)
        self.assertLessEqual(intensity[1].max(), self.mean2 + max_deviation2)
        self.assertGreaterEqual(intensity[1].min(),
                                self.mean2 - max_deviation2)
        self.assertLessEqual(intensity[2].max(), self.mean3 + max_deviation3)
        self.assertGreaterEqual(intensity[2].min(),
                                self.mean3 - max_deviation3)

        for i in (3, 4, 5, 6):
            self.assertLessEqual(intensity[i].max(),
                                 self.mean4567 + max_deviation4567)
            self.assertGreaterEqual(intensity[i].min(),
                                    self.mean4567 - max_deviation4567)

        assert_allclose(intensity.mean(axis=1),
                        [self.mean1, self.mean2, self.mean3] +
                        [self.mean4567] * 4,
                        rtol=5e-2)

        self.assertLess(intensity[0].std(), self.stddev1)
        self.assertLess(intensity[1].std(), self.stddev2)
        self.assertLess(intensity[2].std(), self.stddev3)
        self.assertLess(intensity[3].std(), self.stddev45)
        self.assertLess(intensity[4].std(), self.stddev45)
        self.assertLess(intensity[5].std(), self.stddev67)
        self.assertLess(intensity[6].std(), self.stddev67)