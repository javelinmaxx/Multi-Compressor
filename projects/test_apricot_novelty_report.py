#!/usr/bin/env python3
"""Independent known-signal calibration for novelty-to-phase measurements."""
import unittest
import numpy as np
from generate_apricot_novelty_report import positive_novelty, spectral_residual, phase_targets


class NoveltyHandoffCalibration(unittest.TestCase):
    def test_neighbour_protection_floor_and_strict_magnitude_gate(self):
        actual = positive_novelty(np.array([2.,2.,.02]),np.array([1.,2.,0.]),2.,.1)
        np.testing.assert_allclose(actual,[.7,0,0],atol=1e-12)
        self.assertEqual(positive_novelty(np.array([.02]),np.array([0.]),2.,0.)[0],0)

    def test_residual_preserves_new_partial_phase_and_rejects_background(self):
        n = 128; time = np.arange(n)
        attack = np.cos(2*np.pi*5*time/n+.3)
        background = 10*np.cos(2*np.pi*2*time/n-.7)
        spectrum = np.fft.fft(attack+background)
        novelty = np.zeros(n); novelty[5] = abs(spectrum[5])
        np.testing.assert_allclose(spectral_residual(spectrum,novelty),attack,atol=1e-12)
        np.testing.assert_allclose(spectral_residual(spectrum,np.zeros(n)),0,atol=1e-12)

    def test_anchor_referenced_phase_and_local_delay(self):
        n,rate,anchor,k = 256,48000,37,11
        expected_angle,expected_delay = .7,.0002
        spectrum = np.zeros(n,dtype=complex)
        for j in [k-1,k,k+1]:
            phase = -2*np.pi*j*anchor/n-expected_angle-2*np.pi*(j-k)*rate/n*expected_delay
            spectrum[j] = 2*np.exp(1j*phase)
        angle,delay = phase_targets(spectrum,np.array([k]),anchor,1,rate)
        np.testing.assert_allclose(angle,expected_angle,atol=1e-12)
        np.testing.assert_allclose(delay,expected_delay,atol=1e-12)
        changed,_ = phase_targets(spectrum,np.array([k]),anchor+7,1,rate)
        self.assertGreater(abs(np.angle(np.exp(1j*(changed[0]-angle[0])))),.1)
        negative,_ = phase_targets(spectrum,np.array([k]),anchor,-1,rate)
        np.testing.assert_allclose(abs(np.angle(np.exp(1j*(negative-angle)))),np.pi,atol=1e-12)


if __name__ == "__main__":
    unittest.main()
