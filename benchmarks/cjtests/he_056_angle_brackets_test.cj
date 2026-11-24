import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve("<>"), true)
}

@Test
func case1() {
    @Expect(typed_fn_solve("<<><>>"), true)
}

@Test
func case2() {
    @Expect(typed_fn_solve("<><><<><>><>"), true)
}

@Test
func case3() {
    @Expect(typed_fn_solve("<><><<<><><>><>><<><><<>>>"), true)
}

@Test
func case4() {
    @Expect(typed_fn_solve("<<<><>>>>"), false)
}

@Test
func case5() {
    @Expect(typed_fn_solve("><<>"), false)
}

@Test
func case6() {
    @Expect(typed_fn_solve("<"), false)
}

@Test
func case7() {
    @Expect(typed_fn_solve("<<<<"), false)
}

@Test
func case8() {
    @Expect(typed_fn_solve(">"), false)
}

@Test
func case9() {
    @Expect(typed_fn_solve("<<>"), false)
}

@Test
func case10() {
    @Expect(typed_fn_solve("<><><<><>><>><<>"), false)
}

@Test
func case11() {
    @Expect(typed_fn_solve("<><><<><>><>>><>"), false)
}

@Test
func case12() {
    @Expect(typed_fn_solve(""), true)
}

@Test
func case13() {
    @Expect(typed_fn_solve("><"), false)
}

@Test
func case14() {
    @Expect(typed_fn_solve("<<>>"), true)
}

@Test
func case15() {
    @Expect(typed_fn_solve("<><>"), true)
}
