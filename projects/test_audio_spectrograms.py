#!/usr/bin/env python3
"""Calibration checks for sample-derived academic spectrograms."""
import tempfile
from pathlib import Path
import unittest

import numpy as np
from generate_audio_spectrograms import load, phase_pair, stft


class SpectrogramCalibration(unittest.TestCase):
    def test_parseval_and_known_gain(self):
        rng = np.random.default_rng(214)
        audio = rng.normal(0,.1,8192)
        _,_,_,power = stft(audio,48000,1024,256)
        frames = np.lib.stride_tricks.sliding_window_view(audio,1024)[::256]
        window = np.hanning(1024)
        expected = np.sum((frames*window)**2,axis=1)/np.sum(window**2)
        np.testing.assert_allclose(power.sum(axis=0),expected,rtol=1e-12)
        _,_,_,quiet = stft(audio*.5,48000,1024,256)
        np.testing.assert_allclose(10*np.log10(quiet/power),-6.020599913,atol=1e-8)

    def test_phase_difference_and_silence(self):
        size = 1024
        time = np.arange(4096)
        x = np.cos(2*np.pi*32*time/size)
        y = np.cos(2*np.pi*32*time/size+np.pi/2)
        _,_,before,_ = stft(x,48000,size,128)
        _,_,after,_ = stft(y,48000,size,128)
        _,_,delta = phase_pair(before,after)
        np.testing.assert_allclose(delta[32],np.pi/2,atol=1e-6)
        silence = np.zeros_like(before)
        self.assertEqual(phase_pair(silence,silence)[2].count(),0)
        self.assertEqual(phase_pair(before,silence)[2].count(),0)

    def test_mid_side_energy(self):
        time = np.arange(4096)
        left = np.cos(2*np.pi*32*time/1024)
        right = np.cos(2*np.pi*64*time/1024)
        powers = [stft(x,48000,1024,256)[3].sum(axis=0)
                  for x in [left,right,(left+right)/2,(left-right)/2]]
        np.testing.assert_allclose(powers[2]+powers[3],(powers[0]+powers[1])/2,rtol=1e-12)
        self.assertTrue(np.all(stft((left-left)/2,48000,1024,256)[3] == 0))

    def test_reject_invalid_capture(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"capture.csv"
            path.write_text("sample,rate,input\n0,48000,0\n2,48000,0\n")
            with self.assertRaises(ValueError): load(path)
            path.write_text("sample,rate,input\n0,48000,0\n1,48000,nan\n")
            with self.assertRaises(ValueError): load(path)
            path.write_text("sample,rate,input\n0,48000,0\n1,44100,0\n")
            with self.assertRaises(ValueError): load(path)
        with self.assertRaises(ValueError): stft(np.zeros(5),48000)


if __name__ == "__main__":
    unittest.main()
