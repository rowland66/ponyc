interface val Decoder
  fun decode(b:U32): (U32, U8)

class DecoderBytes
  """
  A class that maintains a U32 that can be loaded with bytes from a byte stream and passed to the decode function.
  """
  var _decode_bytes: U32 = 0
  var _bytes_loaded: U8 = 0

  fun ref pushByte(b: U8) =>
    if _bytes_loaded == 0 then
      _decode_bytes = (_decode_bytes or (b.u32() << 24))
    elseif _bytes_loaded == 1 then
      _decode_bytes = (_decode_bytes or (b.u32() << 16))
    elseif _bytes_loaded == 2 then
      _decode_bytes = (_decode_bytes or (b.u32() << 8))
    elseif _bytes_loaded == 3 then
      _decode_bytes = _decode_bytes or b.u32()
    else
      return
    end
    _bytes_loaded = _bytes_loaded + 1

  fun bytes_loaded(): U8 =>
    _bytes_loaded

  fun decode_bytes(): U32 =>
    _decode_bytes

  fun ref process_bytes(count: U8) =>
    _decode_bytes = (_decode_bytes <<~ (count * 8).u32())
    _bytes_loaded = _bytes_loaded - count
