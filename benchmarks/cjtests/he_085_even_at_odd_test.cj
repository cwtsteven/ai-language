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
    @Expect(typed_fn_solve(listOf([4, 88])), 88)
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([4, 5, 6, 7, 2, 122])), 122)
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([4, 0, 6, 7])), 0)
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([4, 4, 6, 8])), 12)
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([])), 0)
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([2])), 0)
}

@Test
func case6() {
    @Expect(typed_fn_solve(listOf([1, -4])), -4)
}

@Test
func case7() {
    @Expect(typed_fn_solve(listOf([1, 3, 5])), 0)
}
