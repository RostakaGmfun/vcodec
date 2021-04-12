# vcodec
`vcodec_dct` is a toy codec based on ideas of AVC (H.264) standard, but with lots of simplifications, luma only,
because it is developed *just for fun*.

As of now, only intra-frame compression is implemented, featuring 8x8 integer transform (aka simplified DCT used in H.264) with 4x4
Hadamard transform for DC coefficients and 3 spatial prediction modes.
The inter-frame compression (mostly motion prediction) is WIP, and I hope to finish it *some time later*.

## Usage

Encoding:
```bash
./vcodec-test /path/to/Y4M-luma-only-raw-video > /path/to/encoded-output
```

Decoding:
```bash
./vcodec-dec-test /path/to/encoded-file > /path/to/decoded-y4m
```

You can play Y4M files with `ffplay`, for example.
