primitive UTF8Encoder is Encoder

  fun encode(value: U32): (USize, U8, U8, U8, U8) =>
    """
    Encode the code point into UTF-8. It returns a tuple with the size of the
    encoded data and then the encoded bytes.
    """
    if value < 0x80 then
      (1, value.u8(), 0, 0, 0)
    elseif value < 0x800 then
      ( 2,
        ((value >> 6) or 0xC0).u8(),
        ((value and 0x3F) or 0x80).u8(),
        0,
        0
      )
    elseif value < 0xD800 then
      ( 3,
        ((value >> 12) or 0xE0).u8(),
        (((value >> 6) and 0x3F) or 0x80).u8(),
        ((value and 0x3F) or 0x80).u8(),
        0
      )
    elseif value < 0xE000 then
      // UTF-16 surrogate pairs are not allowed.
      (3, 0xEF, 0xBF, 0xBD, 0)
    elseif value < 0x10000 then
      ( 3,
        ((value >> 12) or 0xE0).u8(),
        (((value >> 6) and 0x3F) or 0x80).u8(),
        ((value and 0x3F) or 0x80).u8(),
        0
      )
    elseif value < 0x110000 then
      ( 4,
        ((value >> 18) or 0xF0).u8(),
        (((value >> 12) and 0x3F) or 0x80).u8(),
        (((value >> 6) and 0x3F) or 0x80).u8(),
        ((value and 0x3F) or 0x80).u8()
      )
    else
      // Code points beyond 0x10FFFF are not allowed.
      (3, 0xEF, 0xBF, 0xBD, 0)
    end
    
  fun tag _add_encoded_bytes(encoded_bytes: Array[U8] ref, data: (USize, U8, U8, U8, U8)) =>
    let s = data._1
    encoded_bytes.push(data._2)
    if s > 1 then
      encoded_bytes.push(data._3)
      if s > 2 then
        encoded_bytes.push(data._4)
        if s > 3 then
          encoded_bytes.push(data._5)
        end
      end
    end

