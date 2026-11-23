import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(10), 55)
}

@Test
func case1() {
    @Expect(typed_fn_solve(1), 1)
}

@Test
func case2() {
    @Expect(typed_fn_solve(8), 21)
}

@Test
func case3() {
    @Expect(typed_fn_solve(11), 89)
}

@Test
func case4() {
    @Expect(typed_fn_solve(12), 144)
}

@Test
func case5() {
    @Expect(typed_fn_solve(0), 0)
}

@Test
func case6() {
    @Expect(typed_fn_solve(2), 1)
}

@Test
func case7() {
    @Expect(typed_fn_solve(3), 2)
}

@Test
func case8() {
    @Expect(typed_fn_solve(4), 3)
}

@Test
func case9() {
    @Expect(typed_fn_solve(5), 5)
}

@Test
func case10() {
    @Expect(typed_fn_solve(6), 8)
}

@Test
func case11() {
    @Expect(typed_fn_solve(7), 13)
}

@Test
func case12() {
    @Expect(typed_fn_solve(9), 34)
}

@Test
func case13() {
    @Expect(typed_fn_solve(13), 233)
}

@Test
func case14() {
    @Expect(typed_fn_solve(14), 377)
}

@Test
func case15() {
    @Expect(typed_fn_solve(15), 610)
}

@Test
func case16() {
    @Expect(typed_fn_solve(16), 987)
}

@Test
func case17() {
    @Expect(typed_fn_solve(17), 1597)
}

@Test
func case18() {
    @Expect(typed_fn_solve(18), 2584)
}

@Test
func case19() {
    @Expect(typed_fn_solve(19), 4181)
}

@Test
func case20() {
    @Expect(typed_fn_solve(20), 6765)
}
