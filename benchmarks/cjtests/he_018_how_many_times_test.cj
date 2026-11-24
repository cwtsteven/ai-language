import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve("", "x"), 0)
}

@Test
func case1() {
    @Expect(typed_fn_solve("xyxyxyx", "x"), 4)
}

@Test
func case2() {
    @Expect(typed_fn_solve("cacacacac", "cac"), 4)
}

@Test
func case3() {
    @Expect(typed_fn_solve("john doe", "john"), 1)
}

@Test
func case4() {
    @Expect(typed_fn_solve("aaa", "aa"), 2)
}

@Test
func case5() {
    @Expect(typed_fn_solve("abc", "abcd"), 0)
}

@Test
func case6() {
    @Expect(typed_fn_solve("a", "a"), 1)
}

@Test
func case7() {
    @Expect(typed_fn_solve("abababa", "aba"), 3)
}
