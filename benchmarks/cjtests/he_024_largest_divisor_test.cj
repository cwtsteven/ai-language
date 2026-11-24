import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(3), 1)
}

@Test
func case1() {
    @Expect(typed_fn_solve(7), 1)
}

@Test
func case2() {
    @Expect(typed_fn_solve(10), 5)
}

@Test
func case3() {
    @Expect(typed_fn_solve(100), 50)
}

@Test
func case4() {
    @Expect(typed_fn_solve(49), 7)
}

@Test
func case5() {
    @Expect(typed_fn_solve(2), 1)
}

@Test
func case6() {
    @Expect(typed_fn_solve(9), 3)
}

@Test
func case7() {
    @Expect(typed_fn_solve(15), 5)
}

@Test
func case8() {
    @Expect(typed_fn_solve(97), 1)
}

@Test
func case9() {
    @Expect(typed_fn_solve(200), 100)
}
