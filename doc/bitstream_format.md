# Bitstream format for vcodec_dct
`vcodec_dct` is a toy codec based on ideas of AVC (H.264) standard, but with lots of simplifications,
because it is developed *just for fun*.

## Top-level structure
The video sequence consists of packets (see NAL units in H.264 terminology).
Each packet is either:
* A sequence parameter set (SPS). Immutable parameters for the whole video sequence.
* A picture parameter set (PPS). Updates mutable parameters (e.g. quantization parameters).
* An I-frame. A frame of the sequence that can be decoded without
  any previous frames (yet it requires valid SPS and PPS at the decoder to be available).
* A P-frame. A frame of the sequence that might reference the last I-frame (but is not obligated to do so).

## SPS format
SPS is short and is present only once, so no special packing/encoding is employed for simplicity.

```c
typedef struct {
    uint32_t magic;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) vcodec_dct_sps_t;
```

## PPS format
TBD.

## Frame format
There are no byte/bit alignment requirements for the start of frame packet.
Each frame is subdivided into 16x16 macroblocks. When frame resolution is not
evenly divisible by 16 in either width or height, it is padded with 8x8 or 4x4 blocks.
The subdivision logic is known for both encoder and decoder, so macroblock size is not
written to the output bitstream.

### Generic header
Bits:

`t`
t - type (0 - I-frame, 1 - P-frame).

### I-Frame macroblock format
Since I-Frame can contain only I-type macroblocks, there is no need to write macroblock type
into the stream when encoding an I-Frame.

Stuff needed for decoding:
* Macroblock prediction mode (none, DC, horizontal, or vertical).
* AC coefficients.
* DC coefficients.

Bits:
`pp`

pp - intra prediction mode:
* 00 - none.
* 01 - DC.
* 10 - horizontal.
* 11 - vertical.

Typical line (15 values):
`5   6   -1   -5    0    0    0    1    1    0    0    0    0    0    0 `

AC coefficients for all blocks:
(exp-golomb coded absolute value minus one, num zeroes until next coefficient or end):
(6), (1, 0), (1, 3), (5, 0), (1, 0), (6, 0), (5) -> inverse
(00110), (0, 0), (0, 011), (001100, 0), (0, 0), (001110, 0), (001100) -> 00111000001110011000000011100001100
(5, 0), (6, 0), (1, 0), (5, 3), (1, 0), (1, 5) -> (binary format)
(001100, 0), (001101, 0), (1, 0), (001100, 0111), (1, 0), (1, 001100) ->   00110000011010100011000111101001100

Sign bits for non-zero coefficients (the length of this bit string is implicitly known from coefficient list):
110011

#### DC block coding



### P-Frame macroblock format
TBD.
