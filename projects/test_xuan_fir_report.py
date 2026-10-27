#!/usr/bin/env python3
"""Independent analytical checks of the FIR report's response convention."""
import unittest
import numpy as np
from generate_xuan_fir_report import response, phase_error


class FIRResponseCalibration(unittest.TestCase):
    def test_delayed_identity_has_zero_phase_and_unity_gain(self):
        for rate in [44100,48000,96000]:
            centre = int(np.rint(rate*.025))
            taps = np.zeros(2*centre+1); taps[centre] = 1
            frequencies = np.geomspace(20,min(20000,rate*.49),63)
            np.testing.assert_allclose(response(taps,frequencies,rate,centre),1,atol=1e-12)

    def test_known_quadrature_and_nonunity_gain(self):
        # The centred antisymmetric three-tap kernel has Hc(w)=j*sin(w).
        measured = response([.5,0,-.5],np.array([12000]),48000,1)
        np.testing.assert_allclose(measured,1j,atol=1e-12)
        np.testing.assert_allclose(phase_error(measured,90),0,atol=1e-12)
        np.testing.assert_allclose(phase_error(measured,0),90,atol=1e-12)
        quiet = response([.25,0,-.25],np.array([12000]),48000,1)
        np.testing.assert_allclose(20*np.log10(abs(quiet)),-6.020599913,atol=1e-8)
        np.testing.assert_allclose(phase_error(np.exp(-1j*np.deg2rad(179)),179),2,atol=1e-10)

    def test_direct_response_matches_fft_on_known_grid(self):
        taps = np.random.default_rng(17).normal(size=17)
        bins = np.array([0,1,23,77,1000,2048])
        frequencies = bins*48000/4096
        expected = np.fft.rfft(taps,n=4096)[bins]*np.exp(2j*np.pi*bins*8/4096)
        np.testing.assert_allclose(response(taps,frequencies,48000,8),expected,atol=1e-12)


if __name__ == "__main__":
    unittest.main()
