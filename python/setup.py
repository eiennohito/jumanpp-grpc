from setuptools import setup

setup(name='jumanpp_grpc',
      version='0.1',
      description='Python code for gRPC bindings for Juman++',
      long_description='Python code for gRPC bindings for Juman++',
      classifiers=[
          'Development Status :: 3 - Alpha',
          'License :: OSI Approved :: Apache2 License',
          'Programming Language :: Python :: 3.6',
          'Topic :: Text Processing :: Linguistic',
      ],
      keywords='nlp japanese morphological analyzer juman jumanpp',
      url='http://github.com/eiennohito/jumanpp-grpc',
      author='Arseny Tolmachev',
      author_email='arseny@kotonoha.ws',
      license='Apache2',
      packages=['jumanpp_grpc'],
      install_requires=[
          'grpcio',
      ],
      include_package_data=True,
      zip_safe=False)