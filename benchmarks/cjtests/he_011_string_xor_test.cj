import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve("111000", "101010"), "010010")
}

@Test
func case1() {
    @Expect(typed_fn_solve("1", "1"), "0")
}

@Test
func case2() {
    @Expect(typed_fn_solve("0101", "0000"), "0101")
}

@Test
func case3() {
    @Expect(typed_fn_solve("0", "1"), "1")
}

@Test
func case4() {
    @Expect(typed_fn_solve("000", "000"), "000")
}

@Test
func case5() {
    @Expect(typed_fn_solve("1111", "1111"), "0000")
}

@Test
func case6() {
    @Expect(typed_fn_solve("10", "01"), "11")
}
