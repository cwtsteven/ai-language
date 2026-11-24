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
    @Expect(typed_fn_solve(listOf([1, 3, 5, 0])), false)
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([1, 3, -2, 1])), false)
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([1, 2, 3, 7])), false)
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([2, 4, -5, 3, 5, 7])), true)
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([1])), false)
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([-3, 9, -1, 3, 2, 30])), true)
}

@Test
func case6() {
    @Expect(typed_fn_solve(listOf([-3, 9, -1, 3, 2, 31])), true)
}

@Test
func case7() {
    @Expect(typed_fn_solve(listOf([-3, 9, -1, 4, 2, 30])), false)
}

@Test
func case8() {
    @Expect(typed_fn_solve(listOf([-3, 9, -1, 4, 2, 31])), false)
}

@Test
func case9() {
    @Expect(typed_fn_solve(listOf([])), false)
}

@Test
func case10() {
    @Expect(typed_fn_solve(listOf([0, 0])), true)
}

@Test
func case11() {
    @Expect(typed_fn_solve(listOf([2, -2])), true)
}

@Test
func case12() {
    @Expect(typed_fn_solve(listOf([-5])), false)
}
