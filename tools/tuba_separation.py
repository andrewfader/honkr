"""Where htdemucs_6s puts a real tuba: the University of Iowa tuba line (the dry takes
`build/honkr-corpus-render iowa-tuba` writes) separated alone, and added to each OTC excerpt of the
song corpus (its six stems summed back into a mix, in memory) at -6 and -12 dB under the mix, ten
seconds in. The separation is song-corpus/scripts/separate_file.py's, call for call. Prints, per
case, the share of the tuba's energy in each stem (for a mix, the stems' difference with and
without it, over the line), and the `bass` stem's centroid and roll-off as features.py measures
them. Only numbers are printed; no audio is written.

    TORCH_HOME=... .../.claude-gpu-audio/bin/python -I tools/tuba_separation.py /path/to/song-corpus RENDER_DIR
"""
import importlib.util
import os
import statistics
import sys

import numpy as np
import soundfile as sf
import torch
from demucs.apply import apply_model
from demucs.pretrained import get_model


def main():
    corpus, renders = sys.argv[1], sys.argv[2]
    spec = importlib.util.spec_from_file_location('features', os.path.join(corpus, 'scripts', 'features.py'))
    features = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(features)
    dev = 'cuda' if torch.cuda.is_available() else 'cpu'
    model = get_model('htdemucs_6s').to(dev).eval()

    def separate(x):
        mix = torch.from_numpy(np.ascontiguousarray(x.T, dtype=np.float32))
        ref = mix.mean(0)
        mu, sd = ref.mean(), ref.std() + 1e-8
        with torch.no_grad():
            out = apply_model(model, ((mix - mu) / sd)[None], device=dev, shifts=1, split=True, overlap=0.25, progress=False)[0]
        out = (out * sd + mu).cpu().numpy()
        return {s: out[j].T for j, s in enumerate(model.sources)}

    def shares(stems):
        e = {s: float((v.astype(np.float64) ** 2).sum()) for s, v in stems.items()}
        return {s: 100 * v / sum(e.values()) for s, v in e.items()}

    def spectrum(x):
        sp = features.spectral(x.mean(1).astype(np.float32))
        return (sp['centroid_hz'], sp['rolloff85_hz']) if sp else (float('nan'), float('nan'))

    def row(name, share, bass):
        print('%-34s %s  bass stem %4.0f / %4.0f Hz' % (name, ' '.join('%s %5.1f%%' % (s, share[s]) for s in ('bass', 'guitar', 'other', 'vocals', 'piano', 'drums')), *bass))

    torch.manual_seed(0)
    ids = sorted({f.rsplit('_', 1)[0] for f in os.listdir(os.path.join(corpus, 'stems')) if f.startswith('OTC_') and f.endswith('_bass.flac')})
    for dynamic in ('mf', 'ff'):
        y, _ = sf.read(os.path.join(renders, 'iowa-tuba-%s-dry_bass.wav' % dynamic), dtype='float32', always_2d=True)
        line = y.mean(1)
        st = separate(np.stack([line, line], 1) * 0.5 / np.abs(line).max())
        row('alone, %s' % dynamic, shares(st), spectrum(st['bass']))
        for level in (-6, -12):
            in_bass, before, after = [], [], []
            for i in ids:
                stems = {s: sf.read(os.path.join(corpus, 'stems', '%s_%s.flac' % (i, s)), dtype='float32', always_2d=True)[0] for s in model.sources}
                mix = sum(stems.values())
                at, n = 10 * 44100, min(len(line), len(mix) - 10 * 44100)
                tuba = np.zeros_like(mix)
                tuba[at:at + n] = np.stack([line[:n], line[:n]], 1)
                tuba *= np.sqrt((mix ** 2).sum() / (tuba ** 2).sum()) * 10 ** (level / 20)
                without, with_ = separate(mix), separate(mix + tuba)
                share = shares({s: (with_[s] - without[s])[at:at + n] for s in model.sources})
                bass = (spectrum(without['bass'][at:at + n]), spectrum(with_['bass'][at:at + n]))
                in_bass.append(share['bass']); before.append(bass[0][0]); after.append(bass[1][0])
                row('%s, %s, %d dB' % (i[:24], dynamic, level), share, bass[1])
            print('  %s at %d dB: share in bass median %.0f%% (%.0f..%.0f); bass stem centroid median %.0f Hz without the tuba, %.0f with it'
                  % (dynamic, level, statistics.median(in_bass), min(in_bass), max(in_bass), statistics.median(before), statistics.median(after)))


if __name__ == '__main__':
    main()
