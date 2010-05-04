;(function(){

var File = Nginx.File,
	prefix = Nginx.prefix

NginxTests.file = function (r)
{
	r.sendHttpHeader('text/plain; charset=utf-8')
	
	Tests.test('tests for Nginx.File object', function (t)
	{
		t.type(File, 'function', 'Nginx.File object')
		
		t.test('rename', function (t)
		{
			var rc = File.rename(prefix + 'nginx-file-rename.txt', prefix + 'nginx-file-rename.renamed.txt')
			t.ne(rc, File.ERROR, 'first rename')
			
			var rc = File.rename(prefix + 'nginx-file-rename.txt', prefix + 'nginx-file-rename.renamed.txt')
			t.eq(rc, File.ERROR, 'second rename')
			
			var rc = File.rename(prefix + 'nginx-file-rename.renamed.txt', prefix + 'nginx-file-rename.txt')
			t.ne(rc, File.ERROR, 'rename back')
		})
		
		t.test('create/delete', function (t)
		{
			var file = File.open(prefix + 'nginx-file-create.txt')
			t.ok(file, 'create')
			t.instance(file, File, 'file object')
			
			var rc = File.remove(prefix + 'nginx-file-create.txt')
			t.ne(rc, File.ERROR, 'delete')
			
			var rc = File.remove(prefix + 'nginx-file-create.txt')
			t.eq(rc, File.ERROR, 'second delete')
		})
		
		t.test('unicode read/write', function (t)
		{
			var fname = prefix + 'nginx-file-write.txt',
				str = 'Тра-ля-ля-ля и Тру-ля-ля!'
			
			var file = File.open(fname)
			t.ok(file, 'create')
			
			var rc = file.write(str)
			t.ok(rc, 'write')
			
			var file = File.open(fname)
			t.ok(file, 'open')
			
			var size = file.size
			t.gte(size, str.length, 'size')
			
			var res = file.read(size)
			t.eq(res, str, 'read')
			
			var rc = File.remove(fname)
			t.ne(rc, File.ERROR, 'delete')
			
			t.eq(File.readLeaks, 0, 'read leaks')
		})
		
		t.test('large read/write', function (t)
		{
			// 50 * 2000 = 100'000
			var str = new Array(2000).join('xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'),
				fname = prefix + 'nginx-file-write.txt'
			
			var file = File.open(fname)
			t.ok(file, 'create')
			
			var rc = file.write(str)
			t.ok(rc, 'write')
			
			var file = File.open(fname)
			t.ok(file, 'open')
			
			var size = file.size
			t.gte(size, str.length, 'size')
			
			var res = file.read(size)
			t.eq(res, str, 'read')
			
			var rc = File.remove(fname)
			t.ne(rc, File.ERROR, 'delete')
			
			t.eq(File.readLeaks, 0, 'read leaks')
		})
	})
	Tests.oncomplete = function ()
	{
		r.puts('all done')
		r.flush()
		r.sendSpecial(Nginx.HTTP_LAST)
	}
	Tests.run(r)
	
	return Nginx.OK
}

})();