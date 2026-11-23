import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(2), 4)
}

@Test
func case1() {
    @Expect(typed_fn_solve(3), 9)
}

@Test
func case2() {
    @Expect(typed_fn_solve(4), 16)
}

@Test
func case3() {
    @Expect(typed_fn_solve(8), 64)
}

@Test
func case4() {
    @Expect(typed_fn_solve(10), 100)
}

@Test
func case5() {
    @Expect(typed_fn_solve(0), 0)
}

@Test
func case6() {
    @Expect(typed_fn_solve(1), 1)
}

@Test
func case7() {
    @Expect(typed_fn_solve(5), 25)
}

@Test
func case8() {
    @Expect(typed_fn_solve(7), 49)
}

@Test
func case9() {
    @Expect(typed_fn_solve(11), 121)
}

@Test
func case10() {
    @Expect(typed_fn_solve(23), 529)
}

@Test
func case11() {
    @Expect(typed_fn_solve(100), 10000)
}

@Test
func case12() {
    @Expect(typed_fn_solve(1000), 1000000)
}

@Test
func case13() {
    @Expect(typed_fn_solve(1000000), 1000000000000)
}
