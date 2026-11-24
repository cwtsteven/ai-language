import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(""), true)
}

@Test
func case1() {
    @Expect(typed_fn_solve("aba"), true)
}

@Test
func case2() {
    @Expect(typed_fn_solve("aaaaa"), true)
}

@Test
func case3() {
    @Expect(typed_fn_solve("zbcd"), false)
}

@Test
func case4() {
    @Expect(typed_fn_solve("xywyx"), true)
}

@Test
func case5() {
    @Expect(typed_fn_solve("xywyz"), false)
}

@Test
func case6() {
    @Expect(typed_fn_solve("xywzx"), false)
}

@Test
func case7() {
    @Expect(typed_fn_solve("a"), true)
}

@Test
func case8() {
    @Expect(typed_fn_solve("ab"), false)
}

@Test
func case9() {
    @Expect(typed_fn_solve("abba"), true)
}

@Test
func case10() {
    @Expect(typed_fn_solve("abcba"), true)
}
