interface val Encoder
  fun encode(value: U32): (USize, U8, U8, U8, U8)
