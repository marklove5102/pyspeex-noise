"""Noise suppression and auto gain with speex."""

# pylint: disable=no-name-in-module
from speex_noise_cpp import create_speex_noise, process_10ms


class AudioProcessor:
    """Auto gain and noise suppression with speex."""

    def __init__(self, auto_gain: float = 4000, noise_suppression: int = -30) -> None:
        """Initialize audio processor."""
        self._speex_noise = create_speex_noise(auto_gain, noise_suppression)

    def process_10ms(self, audio: bytes) -> bytes:
        """Clean 10ms of audio."""
        return process_10ms(self._speex_noise, audio)
