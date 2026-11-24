import std.collection.ArrayList
import std.unittest.*
import std.unittest.testmacro.*

func listOf(items: Array<Int64>): ArrayList<Int64> {
    let out = ArrayList<Int64>()
    for (item in items) {
        out.add(item)
    }
    return out
}

@Test
func case0() {
    @Expect(typed_fn_solve(listOf([])), false)
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([1, 2, -3, 1, 2, -3])), false)
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([1, 2, -4, 5, 6])), true)
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([1, -1, 2, -2, 5, -5, 4, -4])), false)
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([1, -1, 2, -2, 5, -5, 4, -5])), true)
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([1, -2, 2, -2, 5, -5, 4, -4])), true)
}

@Test
func case6() {
    @Expect(typed_fn_solve(listOf([-1])), true)
}

@Test
func case7() {
    @Expect(typed_fn_solve(listOf([5, -3])), false)
}

@Test
func case8() {
    @Expect(typed_fn_solve(listOf([1, 1, 1, -2])), false)
}

@Test
func case9() {
    @Expect(typed_fn_solve(listOf([-20, 20])), true)
}

@Test
func case10() {
    @Expect(typed_fn_solve(listOf([0, 0, 0])), false)
}
