primitive UTF8StringDecoder is StringDecoder

  fun decode(b: U32): (U32, U8) =>
    """
    Decode up to 4 UTF-8 bytes into a unicode code point. It returns a tuple
    with the codepoint (U32) and the number of bytes consumed.
    """
    let err: (U32, U8) = (0xFFFD, 1)

    let b1:U8 = ((b and 0xFF000000) >> 24).u8()
    let b2:U8 = ((b and 0xFF0000) >> 16).u8()
    let b3:U8 = ((b and 0xFF00) >> 8).u8()
    let b4:U8 = (b and 0xFF).u8()

    if b1 < 0x80 then
      // 1-byte
      (b1.u32(), 1)
    elseif b1 < 0xC2 then
      // Stray continuation.
      err
    elseif b1 < 0xE0 then
      // 2-byte
      if b2 == 0 then
        // Not enough bytes.
        err
      else
        if (b2 and 0xC0) != 0x80 then
          // Not a continuation byte.
          err
        else
          (((b1.u32() << 6) + b2.u32()) - 0x3080, 2)
        end
      end
    elseif b1 < 0xF0 then
      // 3-byte.
      if b3 == 0 then
        // Not enough bytes.
        err
      else
        if
          // Not continuation bytes.
          ((b2 and 0xC0) != 0x80) or
          ((b3 and 0xC0) != 0x80) or
          // Overlong encoding.
          ((b1 == 0xE0) and (b2 < 0xA0))
        then
          err
        else
          (((b1.u32() << 12) + (b2.u32() << 6) + b3.u32()) - 0xE2080, 3)
        end
      end
    elseif b1 < 0xF5 then
      // 4-byte.
      if b4 == 0 then
        // Not enough bytes.
        err
      else
        if
          // Not continuation bytes.
          ((b2 and 0xC0) != 0x80) or
          ((b3 and 0xC0) != 0x80) or
          ((b4 and 0xC0) != 0x80) or
          // Overlong encoding.
          ((b1 == 0xF0) and (b2 < 0x90)) or
          // UTF32 would be > 0x10FFFF.
          ((b1 == 0xF4) and (b2 >= 0x90))
        then
          err
        else
          (((b1.u32() << 18) +
            (b2.u32() << 12) +
            (b3.u32() << 6) +
            b4.u32()) - 0x3C82080, 4)
        end
      end
    else
      // UTF32 would be > 0x10FFFF.
      err
    end
