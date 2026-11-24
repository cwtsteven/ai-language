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
    @Expect(typed_fn_solve(listOf([-1, -2, 4, 5, 6])), listOf([4, 5, 6]))
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([5, 3, -5, 2, 3, 3, 9, 0, 123, 1, -10])), listOf([5, 3, 2, 3, 3, 9, 123, 1]))
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([-1, -2])), listOf([]))
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([])), listOf([]))
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([0])), listOf([]))
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([1])), listOf([1]))
}

@Test
func case6() {
    @Expect(typed_fn_solve(listOf([-5, 0, 8])), listOf([8]))
}

@Test
func case7() {
    @Expect(typed_fn_solve(listOf([150, -20])), listOf([150]))
}
