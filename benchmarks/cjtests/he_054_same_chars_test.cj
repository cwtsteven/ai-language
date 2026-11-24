import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve("eabcdzzzz", "dddzzzzzzzddeddabc"), true)
}

@Test
func case1() {
    @Expect(typed_fn_solve("abcd", "dddddddabc"), true)
}

@Test
func case2() {
    @Expect(typed_fn_solve("dddddddabc", "abcd"), true)
}

@Test
func case3() {
    @Expect(typed_fn_solve("eabcd", "dddddddabc"), false)
}

@Test
func case4() {
    @Expect(typed_fn_solve("abcd", "dddddddabcf"), false)
}

@Test
func case5() {
    @Expect(typed_fn_solve("eabcdzzzz", "dddzzzzzzzddddabc"), false)
}

@Test
func case6() {
    @Expect(typed_fn_solve("aabb", "aaccc"), false)
}

@Test
func case7() {
    @Expect(typed_fn_solve("", ""), true)
}

@Test
func case8() {
    @Expect(typed_fn_solve("a", "a"), true)
}

@Test
func case9() {
    @Expect(typed_fn_solve("a", "b"), false)
}

@Test
func case10() {
    @Expect(typed_fn_solve("abc", "cba"), true)
}
