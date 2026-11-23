import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(15), 5)
}

@Test
func case1() {
    @Expect(typed_fn_solve(27), 3)
}

@Test
func case2() {
    @Expect(typed_fn_solve(63), 7)
}

@Test
func case3() {
    @Expect(typed_fn_solve(330), 11)
}

@Test
func case4() {
    @Expect(typed_fn_solve(13195), 29)
}

@Test
func case5() {
    @Expect(typed_fn_solve(4), 2)
}

@Test
func case6() {
    @Expect(typed_fn_solve(6), 3)
}

@Test
func case7() {
    @Expect(typed_fn_solve(8), 2)
}

@Test
func case8() {
    @Expect(typed_fn_solve(9), 3)
}

@Test
func case9() {
    @Expect(typed_fn_solve(12), 3)
}

@Test
func case10() {
    @Expect(typed_fn_solve(16), 2)
}

@Test
func case11() {
    @Expect(typed_fn_solve(49), 7)
}

@Test
func case12() {
    @Expect(typed_fn_solve(77), 11)
}

@Test
func case13() {
    @Expect(typed_fn_solve(121), 11)
}

@Test
func case14() {
    @Expect(typed_fn_solve(143), 13)
}

@Test
func case15() {
    @Expect(typed_fn_solve(2048), 2)
}

@Test
func case16() {
    @Expect(typed_fn_solve(4096), 2)
}

@Test
func case17() {
    @Expect(typed_fn_solve(9999), 101)
}
