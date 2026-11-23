import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(4), false)
}

@Test
func case1() {
    @Expect(typed_fn_solve(6), false)
}

@Test
func case2() {
    @Expect(typed_fn_solve(8), true)
}

@Test
func case3() {
    @Expect(typed_fn_solve(10), true)
}

@Test
func case4() {
    @Expect(typed_fn_solve(11), false)
}

@Test
func case5() {
    @Expect(typed_fn_solve(12), true)
}

@Test
func case6() {
    @Expect(typed_fn_solve(13), false)
}

@Test
func case7() {
    @Expect(typed_fn_solve(16), true)
}

@Test
func case8() {
    @Expect(typed_fn_solve(-10), false)
}

@Test
func case9() {
    @Expect(typed_fn_solve(-1), false)
}

@Test
func case10() {
    @Expect(typed_fn_solve(0), false)
}

@Test
func case11() {
    @Expect(typed_fn_solve(1), false)
}

@Test
func case12() {
    @Expect(typed_fn_solve(2), false)
}

@Test
func case13() {
    @Expect(typed_fn_solve(7), false)
}

@Test
func case14() {
    @Expect(typed_fn_solve(9), false)
}

@Test
func case15() {
    @Expect(typed_fn_solve(14), true)
}

@Test
func case16() {
    @Expect(typed_fn_solve(18), true)
}

@Test
func case17() {
    @Expect(typed_fn_solve(20), true)
}

@Test
func case18() {
    @Expect(typed_fn_solve(99), false)
}

@Test
func case19() {
    @Expect(typed_fn_solve(100), true)
}

@Test
func case20() {
    @Expect(typed_fn_solve(1000), true)
}

@Test
func case21() {
    @Expect(typed_fn_solve(160), true)
}

@Test
func case22() {
    @Expect(typed_fn_solve(89), false)
}

@Test
func case23() {
    @Expect(typed_fn_solve(200), true)
}

@Test
func case24() {
    @Expect(typed_fn_solve(156), true)
}

@Test
func case25() {
    @Expect(typed_fn_solve(43), false)
}

@Test
func case26() {
    @Expect(typed_fn_solve(141), false)
}

@Test
func case27() {
    @Expect(typed_fn_solve(62), true)
}

@Test
func case28() {
    @Expect(typed_fn_solve(-5), false)
}

@Test
func case29() {
    @Expect(typed_fn_solve(182), true)
}

@Test
func case30() {
    @Expect(typed_fn_solve(64), true)
}

@Test
func case31() {
    @Expect(typed_fn_solve(188), true)
}

@Test
func case32() {
    @Expect(typed_fn_solve(96), true)
}

@Test
func case33() {
    @Expect(typed_fn_solve(130), true)
}

@Test
func case34() {
    @Expect(typed_fn_solve(135), false)
}

@Test
func case35() {
    @Expect(typed_fn_solve(24), true)
}

@Test
func case36() {
    @Expect(typed_fn_solve(37), false)
}

@Test
func case37() {
    @Expect(typed_fn_solve(136), true)
}
